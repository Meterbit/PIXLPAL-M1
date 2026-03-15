#ifndef CAL_CLOCKS
#define CAL_CLOCKS

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char classicClockAppRoute[] = "0/0";
extern TaskHandle_t classicClock_Task_H;
extern void calendarClock_App_Task(void *arguments);

#endif