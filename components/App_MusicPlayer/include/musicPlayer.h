#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H


#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtbColors.h"

static const char musicPlayerAppRoute[] = "9/1";

struct MusicPlayer_Data_t {
uint16_t trackNumber = 1;
};

extern MusicPlayer_Data_t musicPlayerData;
extern TaskHandle_t musicPlayer_Task_H;
extern void musicPlayer_App_Task(void *arguments);

#endif