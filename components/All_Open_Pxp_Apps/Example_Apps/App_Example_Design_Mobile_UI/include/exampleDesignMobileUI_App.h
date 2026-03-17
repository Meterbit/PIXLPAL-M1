#ifndef EXAMPLE_DESIGN_UI_H
#define EXAMPLE_DESIGN_UI_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_colors.h"

extern TaskHandle_t exampleDesignMobileUI_Task_H;
extern void exampleDesignMobileUI_Task(void *arguments);

#endif