#include <Arduino.h>
#include <weather.h>

#include <SPI.h> // Built-in
#define ENABLE_GxEPD2_display 1
#include <GxEPD2_BW.h>

#include <U8g2_for_Adafruit_GFX.h>
#include "epaper_fonts.h"
#include <weatherSymbols.h>
#include <common_functions.h>

#include <sunset.h>
#include <moonPhase.h>

#define SCREEN_WIDTH 800 // Set for landscape mode
#define SCREEN_HEIGHT 480

enum alignment
{
    LEFT,
    RIGHT,
    CENTER
};

// Connections for Waveshare ESP32 e-Paper Driver Board
static const uint8_t EPD_BUSY = 25;
static const uint8_t EPD_CS = 15;
static const uint8_t EPD_RST = 26;
static const uint8_t EPD_DC = 27;
static const uint8_t EPD_SCK = 13;
static const uint8_t EPD_MISO = 12; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 14;

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/EPD_CS, /*DC=*/EPD_DC, /*RST=*/EPD_RST, /*BUSY=*/EPD_BUSY)); // B/W display

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void InitDisplay()
{

    display.init(115200, true, 2, false);
    SPI.end();
    SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
    u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
    u8g2Fonts.setFontDirection(0);             // left to right (this is default)
    u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    display.fillScreen(GxEPD_WHITE);
    display.setFullWindow();
}

String WeekdayToString(int day)
{
    switch (day)
    {
    case 0:
        return "Sun";
    case 1:
        return "Mon";
    case 2:
        return "Tue";
    case 3:
        return "Wed";
    case 4:
        return "Thu";
    case 5:
        return "Fri";
    case 6:
        return "Sat";
    default:
        return "";
    }
}

// #########################################################################################
void drawString(int x, int y, String text, alignment align)
{
    int16_t x1, y1; // the bounds of x,y and w and h of the variable 'text' in pixels.
    uint16_t w, h;
    display.setTextWrap(false);
    display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
    if (align == RIGHT)
        x = x - w;
    if (align == CENTER)
        x = x - w / 2;
    u8g2Fonts.setCursor(x, y + h);
    u8g2Fonts.print(text);
}
// #########################################################################################
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int numReadings, boolean auto_scale, boolean barchart_mode, weatherRecord *weatherRecords)
{

#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
#define number_of_dashes 20

    float maxYscale = -10000;
    float minYscale = 10000;
    int last_x, last_y;
    float x2, y2;
    if (auto_scale == true)
    {
        for (int i = 1; i < numReadings; i++)
        {
            if (DataArray[i] >= maxYscale)
                maxYscale = DataArray[i];
            if (DataArray[i] <= minYscale)
                minYscale = DataArray[i];
        }

        if (minYscale < 0)
        {
            if (minYscale != 0)
                minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
            Y1Min = round(minYscale);
        }
        else
        {
            Y1Min = 0;
        }

        if (maxYscale > 40)
        {
            maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
            Y1Max = round(maxYscale + 0.5);
        }
        else if (maxYscale > 30)
        {
            Y1Max = 40;
        }
        else if (maxYscale > 20)
        {
            Y1Max = 30;
        }
        else if (maxYscale > 10)
        {
            Y1Max = 20;
        }
        else if (maxYscale > 0)
        {
            Y1Max = 10;
        }
        else
        {
            maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
            Y1Max = round(maxYscale + 0.5);
        }
    }

    // Draw the graph
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    last_x = x_pos + 1;
    last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
    display.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, GxEPD_BLACK);
    drawString(x_pos + gwidth / 2 - 30, y_pos - 16, title, CENTER);
    // Draw the data
    for (int gx = 1; gx < numReadings; gx++)
    {
        x2 = x_pos + gx * gwidth / (numReadings - 1) - 1; // max_readings is the global variable that sets the maximum data that can be plotted
        y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
        if (barchart_mode)
        {
            display.fillRect(x2, y2, (gwidth / numReadings) - 1, y_pos + gheight - y2 + 2, GxEPD_BLACK);
        }
        else
        {

            // display.drawLine(last_x -2, last_y-2, x2 -2, y2-2, GxEPD_BLACK);
            // display.drawLine(last_x - 1, last_y-1, x2 - 1, y2-1, GxEPD_BLACK);
            // display.drawLine(last_x + 1, last_y+1, x2 + 1, y2+1, GxEPD_BLACK);
            // display.drawLine(last_x + 2, last_y+2, x2 + 2, y2+2, GxEPD_BLACK);
            display.fillTriangle(last_x, last_y + 2, last_x, last_y - 2, x2, y2 + 2, GxEPD_BLACK);
            display.fillTriangle(last_x, last_y - 2, x2, y2 + 2, x2, y2 - 1, GxEPD_BLACK);
            // display.drawLine(last_x, last_y, x2, y2, GxEPD_BLACK);
        }
        last_x = x2;
        last_y = y2;
    }

    // Draw the Y-axis scale
    u8g2Fonts.setFont(u8g2_font_helvB12_tf);
    for (int spacing = 0; spacing <= y_minor_axis; spacing++)
    {
        for (int j = 0; j < number_of_dashes; j++)
        { // Draw dashed graph grid lines
            if (spacing < y_minor_axis)
                display.drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
        }
        if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5)
        {
            drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
        }
        else
        {
            if (Y1Min < 1 && Y1Max < 10)
                drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
            else
                drawString(x_pos - 10, y_pos + gheight * spacing / y_minor_axis, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
        }
    }

    // x axis

    // ticks
    for (int xTick = 0; xTick < numReadings; xTick++)
    {
        // draw tick every 3 hours, devider at midnight
        int xTickPos = x_pos + xTick * gwidth / (numReadings - 1);
        Serial.println(weatherRecords[xTick].time.tm_hour);
        // devide days at midnight
        if (weatherRecords[xTick].time.tm_hour == 0)
        {
            display.drawLine(xTickPos - 1, y_pos, xTickPos - 1, y_pos + gheight + 20, GxEPD_BLACK);
            display.drawLine(xTickPos, y_pos, xTickPos, y_pos + gheight + 20, GxEPD_BLACK);
        }
        else
        {
            display.drawLine(xTickPos, y_pos + gheight + 1, xTickPos, y_pos + gheight + 6, GxEPD_BLACK);
        }

        // show day of week at midday
        if (weatherRecords[xTick].time.tm_hour == 12)
        {
            drawString(x_pos + xTick * gwidth / (numReadings - 1) - 10, y_pos + gheight + 16, WeekdayToString(weatherRecords[xTick].time.tm_wday), CENTER);
        }
    }
}

// #########################################################################################
void DisplayGraphs(weatherRecord *weatherRecords, int numRecords)
{

    // graphing temperature and rain
    float temps[numRecords];
    float rains[numRecords];

    for (size_t i = 0; i < numRecords; i++)
    {
        temps[i] = weatherRecords[i].temperature;
        rains[i] = weatherRecords[i].percentRain;
    }

    int gwidth = 300;
    int gheight = 200;

    int gx = (SCREEN_WIDTH - gwidth * 2) / 3 + 5;
    int gy = 245;
    int gap = gwidth + gx;

    DrawGraph(30, gy, 350, gheight, 10, 30, "Temperature (°C)", temps, numRecords, true, false, weatherRecords);

    DrawGraph(420, gy, 360, gheight, 0, 100, "Rain (%)", rains, numRecords, false, false, weatherRecords);
}

const unsigned char *IconToBitmap(int weatherCode)
{
    switch (weatherCode)
    {
    case 0: // Clear night
        return ClearNight;
        break;
    case 1: // Sunny day
        return SunnyDay;
        break;
    case 2: // Partly cloudy night
        return PartlyCloudyNight;
        break;
    case 3: // Partly cloudy day
        return PartlyCloudyDay;
        break;
    case 4: // Not used
            // return nodata,x,y,iconW,iconH;
        break;
    case 5: // Mist
        return Mist;
        break;
    case 6: // Fog
        return Fog;
        break;
    case 7: // Cloudy

        return Cloudy;
        break;
    case 8: // Overcast
        return Cloudy;
        break;
    case 9: // Light rain shower night
        return LightRainShowerNight;
        break;
    case 10: // Light rain shower day
        return LightRainShowerDay;
        break;
    case 11: // Drizzle
        return Drizzle;
        break;
    case 12: // Light rain
        return LightRain;
        break;
    case 13: // Heavy rain shower night
        return HeavyRainShowerNight;
        break;
    case 14: //  Heavy rain shower day
        return HeavyRainShowerDay;
        break;
    case 15: // Heavy rain
        return HeavyRain;
        break;
    case 16: // Sleet shower night#
        return SleetShowerNight;
        break;
    case 17: // Sleet shower day
        return SleetShowerDay;
        break;
    case 18: // Sleet
        return Sleet;
        break;
    case 19: // Hail shower night
        return HailShowerNight;
        break;
    case 20: // Hail shower day
        return HailShowerDay;
        break;
    case 21: // Hail
        return Hail;
        break;
    case 22: // Light snow shower night
        return LightSnowShowerNight;
        break;
    case 23: // Light snow shower day
        return LightSnowShowerDay;
        break;
    case 24: //  Light snow
        return LightSnow;
        break;
    case 25: // Heavy snow shower night
        return HeavySnowShowerNight;
        break;
    case 26: // Heavy snow shower day
        return HeavySnowShowerDay;
        break;
    case 27: // Heavy snow
        return HeavySnow;
        break;
    case 28: // Thunder shower night
        return ThunderShowerNight;
        break;
    case 29: // Thunder shower day
        return ThunderShowerDay;
        break;
    case 30: //  Thunder
        return Thunder;
        break;
    case 31: // No data received
        return Thunder;
        break;
    }
    return Thunder;
}

void DisplayConditionsSection(int x, int y, int weatherCode, float temperature, String title)
{

    display.drawBitmap(x - 80, y - 80, IconToBitmap(weatherCode), 160, 128, GxEPD_BLACK);

    display.drawRect(x - 86, y - 131, 173, 228, GxEPD_BLACK);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawString(x - 30, y - 90, title, CENTER);

    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    drawString(x - 25, y + 70, String(temperature, 1) + "°C", CENTER); // Show current Temperature
}

void DisplayForecastIcons(weatherRecord *weatherRecords, int numRecords, tm *timeNow)
{
    // big

    int xInc = 172;

    // if the morning, show midday and evening then tomorrow

    // if afternoon, show tomorrow morning and afternoon

    // morning
    if (timeNow->tm_hour < 12)
    {
        int xCurrent = 367 - 109;

        // find midday, then show 6pm then 9am tomorrow
        for (int fIndex = 0; fIndex < numRecords; fIndex++)
        {

            if (weatherRecords[fIndex].time.tm_hour == 12)
            {
                // midday
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex].weatherCode, weatherRecords[fIndex].temperature, "12pm " + WeekdayToString(weatherRecords[fIndex].time.tm_wday));
                xCurrent += xInc;

                // go forward to 6pm
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex + 2].weatherCode, weatherRecords[fIndex + 2].temperature, "6pm " + WeekdayToString(weatherRecords[fIndex + 2].time.tm_wday));
                xCurrent += xInc + 109;

                // go forward to 9am
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex + 5].weatherCode, weatherRecords[fIndex + 5].temperature, "9am " + WeekdayToString(weatherRecords[fIndex + 5].time.tm_wday));
                break;
            }
        }
    }
    // afternoon
    else
    {
        int xCurrent = 367;
        // find 9am tomorrow, then 12 and 6pm
        for (int fIndex = 0; fIndex < numRecords; fIndex++)
        {

            if (weatherRecords[fIndex].time.tm_hour == 9 && weatherRecords[fIndex].time.tm_wday == ((timeNow->tm_wday + 1) % 7))
            {

                // 9am
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex].weatherCode, weatherRecords[fIndex].temperature, "9am " + WeekdayToString(weatherRecords[fIndex].time.tm_wday));
                xCurrent += xInc;
                // go forward to midday
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex + 1].weatherCode, weatherRecords[fIndex + 1].temperature, "12pm " + WeekdayToString(weatherRecords[fIndex + 1].time.tm_wday));
                xCurrent += xInc;

                // go forward to 6pm
                DisplayConditionsSection(xCurrent, 114, weatherRecords[fIndex + 3].weatherCode, weatherRecords[fIndex + 3].temperature, "6pm " + WeekdayToString(weatherRecords[fIndex + 3].time.tm_wday));
                break;
            }
        }
    }
}
void DrawMoon(int x, int y, double phase)
{
    const int diameter = 70;

    // Draw dark part of moon
    display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
    const int number_of_lines = 90;
    for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++)
    {
        double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
        // Determine the edges of the lighted part of the moon
        double Rpos = 2 * Xpos;
        double Xpos1, Xpos2;
        if (phase < 0.5)
        {
            Xpos1 = -Xpos;
            Xpos2 = Rpos - 2 * phase * Rpos - Xpos;
        }
        else
        {
            Xpos1 = Xpos;
            Xpos2 = Xpos - 2 * phase * Rpos + Rpos;
        }
        // Draw light part of moon
        double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
        double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
        double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
        double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
        double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
        double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
        double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
        double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
        display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
        display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
    }
    display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}

void DisplayAstronomySection(tm *timeNow, int xOffset, double latitude, double longitude)
{

    SunSet sun;
    sun.setPosition(latitude, longitude, 0);
    sun.setCurrentDate((timeNow->tm_year + 1900), (timeNow->tm_mon + 1), timeNow->tm_mday);
    int sunrise = static_cast<int>(sun.calcSunrise());
    int sunset = static_cast<int>(sun.calcSunset());

    if (timeNow->tm_isdst > 0)
    {
        sunrise += 60;
        sunset += 60;
    }

    u8g2Fonts.setFont(u8g2_font_helvB24_tf);
    char day_output[5];
    sprintf(day_output, "%02u:%02u", (sunrise / 60), (sunrise % 60));
    drawString(187 + xOffset, 50, day_output, LEFT);
    sprintf(day_output, "%02u:%02u", (sunset / 60), (sunset % 60));
    drawString(187 + xOffset, 80, day_output, LEFT);

    time_t now = time(NULL);
    struct tm *now_utc = gmtime(&now);
    const int day_utc = now_utc->tm_mday;
    const int month_utc = now_utc->tm_mon + 1;
    const int year_utc = now_utc->tm_year + 1900;

    moonData_t moon;

    moonPhase moonPhase;
    moon = moonPhase.getPhase(); // gets the current moon phase ( 1/1/1970 at 00:00:00 UTC )

    Serial.print("Moon phase angle: ");
    Serial.print(moon.angle); // angle is a integer between 0-360
    Serial.println(" degrees.");
    Serial.print("Moon surface lit: ");
    Serial.println(moon.percentLit * 100); // percentLit is a real between 0-1

    double phase = ((double)moon.angle + 180) / 360;

    Serial.print("moon phase 0-1 0=full, 0.5=new, 1 = full");
    Serial.println(phase);
    DrawMoon(157 + xOffset, 90, phase);
}

void DisplayWeather(weatherRecord *weatherRecords, int numRecords, tm *timeNow, double latitude, double longitude)
{

    DisplayGraphs(weatherRecords, numRecords);

    DisplayConditionsSection(86, 114, weatherRecords[0].weatherCode, weatherRecords[0].temperature, "Now (" + WeekdayToString(weatherRecords[0].time.tm_wday) + ")");

    DisplayForecastIcons(weatherRecords, numRecords, timeNow);

    DisplayAstronomySection(timeNow, timeNow->tm_hour < 12 ? (173 * 2) : 0, latitude, longitude);

    display.display(false); // Full screen update mode
}
