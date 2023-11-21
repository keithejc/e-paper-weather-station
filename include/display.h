#pragma once
#include <weather.h>

void InitDisplay();
void DisplayWeather(weatherRecord *weatherRecords, int numRecords, tm *timeNow, double latitude, double longitude);