#ifndef CURRENCY_EXCHANGE_H
#define CURRENCY_EXCHANGE_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char currencyExchangeAppRoute[] = "4/2";

struct Currency_Stat_t {
  String currencyID1;
  String currencyID2;
  String country1;
  String country2;
  char currencyFilePath[50];
  char apiToken[100] = {0};
  uint8_t currency_No;
  int16_t currencyChangeInterval;
};

extern Currency_Stat_t currentCurrencies;
extern TaskHandle_t currencyExchange_Task_H;
extern void currencyExchange_App_Task(void *arguments);

#endif