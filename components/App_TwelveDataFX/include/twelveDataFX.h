#ifndef TWELVEDATA_FX_H
#define TWELVEDATA_FX_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char twelveDataAppRoute[] = "4/3";

struct TwelveDataFX_t {
    char pair[20] = {0};
    char apiToken[100] = {0};
    int16_t updateInterval;
};

extern TwelveDataFX_t twelveDataFX;
extern TaskHandle_t twelveData_Task_H;
extern void twelveData_App_Task(void *arguments);

#endif
