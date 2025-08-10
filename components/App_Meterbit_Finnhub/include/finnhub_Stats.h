#ifndef FINNHUB_STATS_H
#define FINNHUB_STATS_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char finnhubStatsAppRoute[] = "4/0";

struct Stocks_Stat_t {
  String stockID;
  String currency;
  char stockFilePath[50];
  char apiToken[100] = {0};
  uint8_t stock_No;
  int16_t stockChangeInterval;
};

extern Stocks_Stat_t currentStocks;
extern TaskHandle_t finhubStats_Task_H;
extern void finhubStats_App_Task(void *arguments);

#endif