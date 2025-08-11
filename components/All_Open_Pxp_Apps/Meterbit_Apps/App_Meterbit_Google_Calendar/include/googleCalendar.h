#ifndef GOOGLE_CALENDAR_H
#define GOOGLE_CALENDAR_H

// #include <stdio.h>
// #include <stdlib.h>
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char googleCalendarAppRoute[] = "2/0";

static const String CLIENT_ID = "your_client_id.apps.googleusercontent.com";
static const String CLIENT_SECRET = "your_client_secret";

struct GoogleCal_Data_t {
char refreshToken[250] = {0};
bool showEvents = true;
bool showTasks = true;
bool showHolidays = true;
uint16_t themeColor = 0x4249; // Default to OUTER_SPACE color
};

extern GoogleCal_Data_t userGoogleCal;
extern TaskHandle_t googleCal_Task_H;
extern void googleCal_App_Task(void *arguments);

#endif