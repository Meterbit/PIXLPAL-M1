#ifndef ALL_CLOCKS
#define ALL_CLOCKS

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char classicClockAppRoute[] = "0/0";
extern TaskHandle_t classicClock_Task_H;
extern void classicClock_App_Task(void *arguments);

#endif