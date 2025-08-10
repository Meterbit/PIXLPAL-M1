#ifndef WORLD_CLOCK_H
#define WORLD_CLOCK_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char worldClockAppRoute[] = "0/2";

struct WorldClock_Data_t {
String city1;
String city2;
String city3;
String city4;
String city5;
String city6;
};

extern WorldClock_Data_t worldClockCities;
extern TaskHandle_t worldClock_Task_H;
extern void worldClock_App_Task(void *arguments);

#endif