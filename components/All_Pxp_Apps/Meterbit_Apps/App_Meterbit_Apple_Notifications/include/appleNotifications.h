#ifndef APPLE_NOTIFICATIONS_H
#define APPLE_NOTIFICATIONS_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_colors.h"
// #include "encoder.h"
// #include "button.h"

static const char studioLightAppRoute[] = "7/0";

struct AppleNotification_Data_t {
uint8_t notification = 0; 
};

extern AppleNotification_Data_t appleNotificationInfo;
extern TaskHandle_t appleNotification_Task_H;
extern void appleNotifications_App_Task(void *arguments);

#endif