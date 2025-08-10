#ifndef AUD_SPECTRUM_H
#define AUD_SPECTRUM_H

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char audSpecAnalyzerAppRoute[] = "9/2";
extern TaskHandle_t audSpecAnalyzer_Task_H;
extern void audSpecAnalyzer_App_Task(void *arguments);

#endif