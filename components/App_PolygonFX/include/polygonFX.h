#ifndef POLYGON_FX_H
#define POLYGON_FX_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char polygonAppRoute[] = "4/3";

struct PolygonFX_t {
    char pair[20] = {0};
    char apiToken[100] = {0};
    int16_t updateInterval;
};

extern PolygonFX_t polygonFX;
extern TaskHandle_t polygonFX_Task_H;
extern void polygonFX_App_Task(void *arguments);

#endif
