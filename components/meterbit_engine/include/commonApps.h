#ifndef ALL_CLOCKS
#define ALL_CLOCKS

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"



extern TaskHandle_t firmwareUpdate_H;
extern void firmwareUpdateTask(void *arguments);
#endif