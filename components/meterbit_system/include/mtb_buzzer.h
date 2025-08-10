#ifndef BEEP_H
#define BEEP_H

/*
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
*/



enum BEEPING {BEEP_0 = 1, BEEP_1, BEEP_2, BEEP_3, BEEP_4, CLICK_BEEP};
extern SemaphoreHandle_t beep_Duration_Sem;
extern TaskHandle_t beepTaskHandle;

extern void do_beep(uint8_t beep_Type, uint8_t beep_Count = 1);
extern void buzzer_Beep(void *arguments);

#endif