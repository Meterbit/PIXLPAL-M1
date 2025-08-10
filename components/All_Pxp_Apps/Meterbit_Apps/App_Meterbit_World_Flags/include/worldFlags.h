#ifndef STUDIO_LIGHT_H
#define STUDIO_LIGHT_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_colors.h"
// #include "encoder.h"
// #include "button.h"

static const char worldFlagsAppRoute[] = "6/1";

struct WorldFlags_Data_t {
char countryName[100] = "Nigeria";    // 
uint8_t flagChangeIntv = 100;       // 0-255
bool cycleAllFlags = true;         // true or false
bool showCountryName = false;       // true or false
};

extern WorldFlags_Data_t worldFlagsInfo;
extern TaskHandle_t worldFlags_Task_H;
extern void worldFlags_App_Task(void *arguments);

#endif