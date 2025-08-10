#ifndef OPEN_WEATHER_H
#define OPEN_WEATHER_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char openWeatherAppRoute[] = "3/0";

struct OpenWeatherData_t {
  char location[150] = {0};
};

extern OpenWeatherData_t currentOpenWeatherData;
extern TaskHandle_t openWeather_Task_H;
extern void openWeather_App_Task(void *arguments);

#endif