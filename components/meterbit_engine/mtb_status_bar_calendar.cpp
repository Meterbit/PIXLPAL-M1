#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_ntp.h"
#include "mtb_graphics.h"
#include "mtb_engine.h"

EXT_RAM_BSS_ATTR TaskHandle_t statusBarCalendar_H = NULL;

EXT_RAM_BSS_ATTR Mtb_Services *mtb_Status_Bar_Calendar_Sv = new Mtb_Services(mtb_StatusBar_Calendar_Task, &statusBarCalendar_H, "StatBar Cal Serv.", 4096, 1, pdTRUE);

void mtb_StatusBar_Calendar_Task(void* dService){
  Mtb_Services *thisServ = (Mtb_Services *)dService;
  Mtb_Applications::currentRunningApp->showStatusBarCalendar = pdTRUE;
    uint8_t timeRefresh = pdTRUE;
    mtb_Read_Nvs_Struct("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
    Mtb_FixedText_t weekday_Obj(35, 1, Terminal6x8, clk_Updt.weekDayColour);
    Mtb_FixedText_t mnth_day_Obj(93, 1, Terminal6x8, clk_Updt.dateColour);
    char rtc_WkDay[15] = {0};
    char rtc_Dated[10] = {0};
    time_t present = 0;
    struct tm *now = nullptr;

    uint8_t pre_WeekDay = 111;
    uint8_t pre_Day = 111;
    uint8_t pre_Month = 111;

  while (MTB_SERV_IS_ACTIVE == pdTRUE){
    setenv("TZ", ntp_TimeZone, 1);
    tzset();
    time(&present);
    now = localtime(&present);
  if((now->tm_year) < 124){
    //ESP_LOGI(TAG, "Time is not correct. The year is: %d\n", now->tm_year);
  }else{

if(pre_Day != now->tm_mday  || timeRefresh){

  pre_WeekDay = now->tm_wday;
  pre_Day = now-> tm_mday;
  
  switch (pre_WeekDay){
  case SUN: strcpy(rtc_WkDay, "Sunday");
    break;
  case MON: strcpy(rtc_WkDay, "Monday");
    break;
  case TUE: strcpy(rtc_WkDay, "Tuesday");
    break;
  case WED: strcpy(rtc_WkDay, "Wednesday");
    break;
  case THU: strcpy(rtc_WkDay, "Thursday");
    break;
  case FRI: strcpy(rtc_WkDay, "Friday");
    break;
  case SAT: strcpy(rtc_WkDay, "Saturday");
    break;
    default: strcpy(rtc_WkDay, "ERR");
  }
  if (pre_Day < 10){
		rtc_Dated[0] = '0';
    sprintf(&rtc_Dated[1], "%d",pre_Day);
	}
	else {
    sprintf(rtc_Dated, "%d", pre_Day );
	}
    rtc_Dated[2] = ' ';
    rtc_Dated[6] = ' ';
    rtc_Dated[9] = 0;
    weekday_Obj.mtb_Write_String(rtc_WkDay);
  }

if (pre_Month != now->tm_mon  || timeRefresh){
pre_Month = now->tm_mon;

switch (pre_Month){
      case JANUARY: strcpy(&rtc_Dated[0], "Jan");
      break;
      case FEBUARY: strcpy(&rtc_Dated[0], "Feb");
      break;
      case MARCH: strcpy(&rtc_Dated[0], "Mar");
      break;
      case APRIL: strcpy(&rtc_Dated[0], "Apr");
      break;
      case MAY: strcpy(&rtc_Dated[0], "May");
      break;
      case JUNE: strcpy(&rtc_Dated[0], "Jun");
      break;
      case JULY: strcpy(&rtc_Dated[0], "Jul");
      break;
      case AUGUST: strcpy(&rtc_Dated[0], "Aug");
      break;
      case SEPTEMBER: strcpy(&rtc_Dated[0], "Sep");
      break;
      case OCTOBER: strcpy(&rtc_Dated[0], "Oct");
      break;
      case NOVEMBER: strcpy(&rtc_Dated[0], "Nov");
      break;
      case DECEMBER: strcpy(&rtc_Dated[0], "Dec");
      break;
      default: strcpy(&rtc_Dated[0], "Err");
}
}

if(pre_Day != now->tm_mday  || timeRefresh){

  pre_Day = now-> tm_mday;
  
  if (pre_Day < 10){
		rtc_Dated[4] = '0';
    sprintf(&rtc_Dated[5], "%d",pre_Day);
	}
	else {
    sprintf(&rtc_Dated[4], "%d", pre_Day );
	}
    rtc_Dated[3] = ' ';
    rtc_Dated[6] = 0;
    mnth_day_Obj.mtb_Write_String(rtc_Dated);
  }

  }
  if(xQueueReceive(clock_Update_Q, &clk_Updt, 0)){
  timeRefresh = pdTRUE;
  } else{
    //ESP_LOGI(TAG, "Hello Status Clock\n");
    timeRefresh = pdFALSE;
    vTaskDelay(pdMS_TO_TICKS(1000));  
  }
}

mtb_Delete_This_Service(thisServ);
}






