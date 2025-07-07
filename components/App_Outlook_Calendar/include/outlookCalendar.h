#ifndef OUTLOOK_CALENDAR_H
#define OUTLOOK_CALENDAR_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char outlookCalendarAppRoute[] = "2/1";

struct OutlookCal_Data_t {
char refreshToken[250] = {0};
};

extern OutlookCal_Data_t userOutlookCal;
extern TaskHandle_t outlookCal_Task_H;
extern void outlookCal_App_Task(void *arguments);

#endif