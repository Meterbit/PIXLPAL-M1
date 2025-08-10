#ifndef STOPWATCH_H
#define STOPWATCH_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char stopWatchAppRoute[] = "0/4";

struct RealStopWatch_Data_t {
uint64_t duration = 60;
};

extern RealStopWatch_Data_t frequentStopwatchTime;
extern TaskHandle_t chatGPT_Task_H;
extern void realStopwatch_App_Task(void *arguments);

#endif