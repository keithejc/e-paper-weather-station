#include <Arduino.h>
#include <credentials.h>
#include <constants.h>

#include <ArduinoJson.h>
#include <WiFi.h> // Built-in
#include <HTTPClient.h>
#include "time.h" // Built-in

#include <weather.h>
#include <display.h>

String Time_str, Date_str; // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;

long SleepDuration = 10; // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int WakeupTime = 7;      // Don't wakeup until after 07:00 to save battery power
int SleepTime = 23;      // Sleep after (23+1) 00:00 to save battery power

struct tm timeinfo;

const int maxNumRecords = 40; // 5 days 3 hourly
int numRecordsReceived = 0;
weatherRecord weatherRecords[maxNumRecords];

bool Get5DayWeatherRecord(WiFiClient &client)
{
    HTTPClient http;

    // http.begin(uri,test_root_ca); //HTTPS example connection
    http.begin(client, "datapoint.metoffice.gov.uk", 80, metOfficeUri + apikey);
    int httpCode = http.GET();
    Serial.print("httpCode: ");
    Serial.println(httpCode);
    if (httpCode == HTTP_CODE_OK)
    {
        WiFiClient json = http.getStream();
        DynamicJsonDocument doc(35 * 1024);

        DeserializationError error = deserializeJson(doc, json);
        // Test if parsing succeeds.
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            return false;
        }
        client.stop();
        http.end();

        JsonObject root = doc.as<JsonObject>();
        JsonObject forecastRoot = root["SiteRep"]["DV"]["Location"];

        Serial.println("Loading json");

        int recordIndex = 0;
        for (JsonObject day : forecastRoot["Period"].as<JsonArray>())
        {
            Serial.print("Day ");
            String val = day["value"];
            Serial.println(val);
            for (JsonObject repItem : day["Rep"].as<JsonArray>())
            {
                String temp = repItem["T"];
                weatherRecords[recordIndex].temperature = temp.toFloat();
                String rain = repItem["Pp"];
                weatherRecords[recordIndex].percentRain = rain.toFloat();
                String weatherCode = repItem["W"];
                weatherRecords[recordIndex].weatherCode = weatherCode.toInt();

                struct tm tm = {0};
                strptime(day["value"], "%Y-%m-%d", &tm);
                String mins = repItem["$"];

                int hour = mins.toInt() / 60;
                tm.tm_hour = hour;
                weatherRecords[recordIndex].time = tm;
                recordIndex++;
            }
        }

        numRecordsReceived = recordIndex;
    }
    else
    {
        Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
        Serial.println();
        client.stop();
        http.end();
        return false;
    }
    http.end();

    return true;
}

uint8_t StartWiFi()
{
    Serial.print("\r\nConnecting to: ");
    Serial.println(String(ssid));
    IPAddress dns(8, 8, 8, 8); // Google DNS
    WiFi.disconnect();
    WiFi.mode(WIFI_STA); // switch off AP
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    uint8_t connectionStatus;
    bool AttemptConnection = true;
    while (AttemptConnection)
    {
        connectionStatus = WiFi.status();
        if (millis() > start + 15000)
        { // Wait 15-secs maximum
            AttemptConnection = false;
        }
        if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED)
        {
            AttemptConnection = false;
        }
        delay(50);
    }
    if (connectionStatus == WL_CONNECTED)
    {
        wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
        Serial.println("WiFi connected at: " + WiFi.localIP().toString());
    }
    else
        Serial.println("WiFi connection *** FAILED ***");
    return connectionStatus;
}
// #########################################################################################
void StopWiFi()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}
void BeginSleep()
{
    long SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec)); // Some ESP32 are too fast to maintain accurate time
    esp_sleep_enable_timer_wakeup((SleepTimer + 20) * 1000000LL);                              // Added extra 20-secs of sleep to allow for slow ESP32 RTC timers

    Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
    Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
    Serial.println("Starting deep-sleep period...");
    esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}

boolean UpdateLocalTime()
{
    char time_output[30], day_output[30], update_time[30];
    while (!getLocalTime(&timeinfo, 10000))
    { // Wait for 10-sec for time to synchronise
        Serial.println("Failed to obtain time");
        return false;
    }
    CurrentHour = timeinfo.tm_hour;
    CurrentMin = timeinfo.tm_min;
    CurrentSec = timeinfo.tm_sec;
    sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo); // Creates: '14:05:49'
    sprintf(time_output, "%s %s", "Updated", update_time);
    Date_str = day_output;
    Time_str = time_output;
    return true;
}

boolean SetupTime()
{
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
    setenv("TZ", Timezone, 1);                                                 // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
    tzset();                                                                   // Set the TZ environment variable
    delay(100);
    bool TimeStatus = UpdateLocalTime();
    return TimeStatus;
}

void setup()
{
    StartTime = millis();
    Serial.begin(115200);

    Serial.print("Start");

    if (StartWiFi() == WL_CONNECTED && SetupTime() == true)
    {
        Serial.println("Wifi started");

        InitDisplay(); // Give screen time to initialise by getting weather data!
        byte Attempts = 1;
        bool gotWeather = false;
        WiFiClient client; // wifi client object
        while (gotWeather == false && Attempts <= 2)
        { // Try up-to 2 time for Weather and Forecast data
            gotWeather = Get5DayWeatherRecord(client);
            Serial.print("got weather? ");
            Serial.println(gotWeather);
            Attempts++;
        }
        if (gotWeather)
        {               // Only if received both Weather or Forecast proceed
            StopWiFi(); // Reduces power consumption while displaying weather data

            DisplayWeather(weatherRecords, numRecordsReceived, &timeinfo, HomeLatitude, HomeLongitude);
        }
        //}
    }
    BeginSleep();
}

void loop()
{
    // put your main code here, to run repeatedly:
}