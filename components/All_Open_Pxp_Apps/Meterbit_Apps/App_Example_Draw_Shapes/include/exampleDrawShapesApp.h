#ifndef STUDIO_LIGHT_H
#define STUDIO_LIGHT_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_colors.h"

extern TaskHandle_t exampleDrawShapeApp_Task_H;
extern void exampleDrawShapeApp_Task(void *arguments);

#endif