#ifndef STUDIO_LIGHT_H
#define STUDIO_LIGHT_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtbColors.h"
// #include "encoder.h"
// #include "button.h"

static const char studioLightAppRoute[] = "6/0";

struct StudioLight_Data_t {
uint16_t studioLightBrightness = 100; // 0-255
uint16_t studioLightChangeSpeed = 100;      // 0-255
uint16_t studioLightColor[10] = {WHITE, PURPLE, GREEN, YELLOW, BLUE, MAGENTA, PALE_AZURE, EBONY, SWEET_BROWN, SKOBELOFF}; // 
uint8_t studioLightColorPattern = 0; 
};

extern StudioLight_Data_t studioLightsInfo;
extern TaskHandle_t studioLight_Task_H;
extern void studioLight_App_Task(void *arguments);

#endif