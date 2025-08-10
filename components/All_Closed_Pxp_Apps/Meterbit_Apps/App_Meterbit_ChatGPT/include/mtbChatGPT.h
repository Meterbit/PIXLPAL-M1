#ifndef CHAT_GPT_H
#define CHAT_GPT_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char chatGPTAppRoute[] = "8/0";

extern TaskHandle_t chatGPT_Task_H;
extern void chatGPT_App_Task(void *arguments);

#endif