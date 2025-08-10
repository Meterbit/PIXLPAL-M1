#ifndef INTERNET_RADIO
#define INTERNET_RADIO


#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include <Arduino.h>

static const char internetRadioAppRoute[] = "9/0";

struct RadioStation_t {
  char stationName[100];
  char streamLink[1000];
  int serialNumber;
};

extern RadioStation_t currentRadioStation;
extern TaskHandle_t internet_Radio_Task_H;
extern void internetRadio_App_Task(void *arguments);
#endif