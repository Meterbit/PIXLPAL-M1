#ifndef LIVE_FOOTBALL_H
#define LIVE_FOOTBALL_H

// #include <stdio.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

#define FIXTURES_ENDPOINT 0
#define STANDINGS_ENDPOINT 1
#define LIVE_MATCHES_ENDPOINT 2

static const char liveFootbalAppRoute[] = "5/0";

// Your API-Football subscription key from RapidAPI
static const char *API_KEY = "Place your api key here";

struct LiveFootball_Data_t {
  uint8_t endpointType = 0; // 0: Fixture; 1: Standings
  uint16_t leagueID = 39; // Default league ID for API-Football
  char userAPI_Token[100] = {0}; // User's API token for authentication
};

extern LiveFootball_Data_t liveFootballData;
extern TaskHandle_t liveFootball_Task_H;
extern void liveFootball_App_Task(void *arguments);

#endif