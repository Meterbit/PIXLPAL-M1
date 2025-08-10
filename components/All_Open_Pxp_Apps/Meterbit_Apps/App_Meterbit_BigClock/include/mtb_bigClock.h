#ifndef BIG_CLOCK_H
#define BIG_CLOCK_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char bigClockCalendarAppRoute[] = "0/3";

extern TaskHandle_t bigClockCalendar_Task_H;
extern void bigClock_App_Task(void *arguments);

#endif