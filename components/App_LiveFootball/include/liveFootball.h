#ifndef LIVE_FOOTBALL_H
#define LIVE_FOOTBALL_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char liveFootbalAppRoute[] = "5/0";

struct LiveFootball_Data_t {
  char endpointType[20] = "fixtures"; // 1: Fixture; 2: Standings
  uint16_t leagueID = 39; // Default league ID for API-Football
};

extern LiveFootball_Data_t liveFootballData;
extern TaskHandle_t liveFootball_Task_H;
extern void liveFootball_App_Task(void *arguments);

#endif