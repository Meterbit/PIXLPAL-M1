#ifndef CRYPTO_STATS_H
#define CRYPTO_STATS_H

// #include <stdio.h>
// #include <stdlib.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "encoder.h"
// #include "button.h"

static const char cryptoStatsAppRoute[] = "4/1";

struct Crypto_Stat_t {
  String coinID;
  String coinSymbol;
  String currency;
  char coinFilePath[50];
  char apiToken[150] = {0};
  uint8_t coin_No;
  int16_t cryptoChangeInterval;
};

extern Crypto_Stat_t currentCryptoCurrency;
extern TaskHandle_t chatGPT_Task_H;
extern void chatGPT_App_Task(void *arguments);

#endif