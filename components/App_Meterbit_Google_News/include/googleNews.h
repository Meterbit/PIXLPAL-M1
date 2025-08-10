#ifndef GOOGLE_NEWS_H
#define GOOGLE_NEWS_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char googleNewsAppRoute[] = "1/0";
extern TaskHandle_t googleNews_Task_H;
extern void googleNews_App_Task(void *arguments);

#endif