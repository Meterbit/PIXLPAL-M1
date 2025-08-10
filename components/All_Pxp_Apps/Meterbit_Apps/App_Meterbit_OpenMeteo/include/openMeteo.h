#ifndef OPENMETEO_H
#define OPENMETEO_H

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char openMeteoAppRoute[] = "3/1";

struct OpenMeteoData_t {
  char location[150] = {0};
};

extern OpenMeteoData_t currentOpenMeteoData;
extern TaskHandle_t openMeteo_Task_H;
extern void openMeteo_App_Task(void *arguments);

#endif