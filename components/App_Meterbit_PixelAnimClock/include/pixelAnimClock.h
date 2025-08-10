#ifndef PIX_ANIM_CLOCK
#define PIX_ANIM_CLOCK

// #include <stdio.h>
// #include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char pixelAnimClockAppRoute[] = "0/1";

struct PixAnimClkSettings_t{
    char headerText[200];
    uint16_t headerTextColor;
    uint16_t themeColor[3];
    uint16_t animInterval;
};

extern PixAnimClkSettings_t savedPixAnimClkSet;
extern TaskHandle_t pixAnimClock_Task_H;
extern void pixAnimClock_App_Task(void *arguments);

#endif