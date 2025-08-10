#ifndef GOOGLE_WEATHER_H
#define GOOGLE_WEATHER_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char googleWeatherAppRoute[] = "3/2";

struct GoogleWeatherData_t {
  char location[150] = {0};
};

extern GoogleWeatherData_t currentGoogleWeatherData;
extern TaskHandle_t googleWeather_Task_H;
extern void googleWeather_App_Task(void *arguments);

#endif