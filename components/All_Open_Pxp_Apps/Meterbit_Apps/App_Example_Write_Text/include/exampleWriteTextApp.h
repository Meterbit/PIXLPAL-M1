#ifndef STUDIO_LIGHT_H
#define STUDIO_LIGHT_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_colors.h"
// #include "encoder.h"
// #include "button.h"

static const char exampleAppRouteRoute[] = "10/0";

struct ExampleApp_Data_t {
uint16_t exampleAppTextColor = GREEN_BLUE; // 0-255
};

extern ExampleApp_Data_t exampleAppInfo;
extern TaskHandle_t exampleApp_Task_H;
extern void exampleApp_Task(void *arguments);

#endif