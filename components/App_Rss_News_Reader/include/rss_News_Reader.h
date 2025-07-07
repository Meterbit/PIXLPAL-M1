#ifndef RSS_NEWS_READER_H
#define RSS_NEWS_READER_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char rssNewsAppRoute[] = "1/1";
extern TaskHandle_t rssNewsApp_Task_H;
extern void rssNewsApp_Task(void* dApp);

#endif