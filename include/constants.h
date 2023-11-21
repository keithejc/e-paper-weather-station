#pragma once
#include <Arduino.h>

const double HomeLatitude = 51.481312; // where you at; for doing moon phase
const double HomeLongitude = -3.180500;

const String metOfficeUri = "/public/data/val/wxfcs/all/json/350759?res=3hourly&key="; // replace 350759 nwith your location

const char *Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02"; // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
                                                       // See below for examples
const char *ntpServer = "0.uk.pool.ntp.org";           // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                       // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                       // EU "0.europe.pool.ntp.org"
                                                       // US "0.north-america.pool.ntp.org"
                                                       // See: https://www.ntppool.org/en/
int gmtOffset_sec = 0;                                 // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int daylightOffset_sec = 3600;                         // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

// Day of the week
const char *weekday_D[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Month
const char *month_M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
