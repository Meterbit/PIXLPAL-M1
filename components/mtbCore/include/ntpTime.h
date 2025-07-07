#ifndef NTP_TIME_H
#define NTP_TIME_H

#define RESTART 0;

#include "ledPanel.h"

extern TaskHandle_t sntp_Time_handle;
extern void sntp_Time_init_Task(void *params);
enum WEEKDAYS {SUN = 0, MON, TUE, WED, THU, FRI, SAT};
enum MONTHS {JANUARY = 0,FEBUARY, MARCH, APRIL, MAY,JUNE,JULY, AUGUST, SEPTEMBER, OCTOBER,NOVEMBER, DECEMBER};

#endif