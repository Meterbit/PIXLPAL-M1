#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ntpTime.h"
#include "ledPanel.h"
#include "mtbApps.h"
#include "classClk.h"
#include "scrollMsgs.h"

EXT_RAM_BSS_ATTR TaskHandle_t statusBarClock_H = NULL;

EXT_RAM_BSS_ATTR Services *statusBarClock_Sv = new Services(statusBarClock, &statusBarClock_H, "StatBar Clk Serv.", 4096, 1, pdTRUE);

void statusBarClock(void* dService){
  Services *thisServ = (Services *)dService;
  Applications::currentRunningApp->showStatusBarClock = pdTRUE;
    uint8_t timeRefresh = pdTRUE;
    read_struct_from_nvs("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
    FixedText_t hr_min_Obj(52, 1, Terminal6x8, clk_Updt.hourMinColour);
    FixedText_t mnth_day_Obj(93, 1, Terminal6x8, clk_Updt.dateColour);
    char rtc_Hr_Min[10] = {0};
    char rtc_Dated[10] = {0};
    time_t present = 0;
    struct tm *now = nullptr;
//    char AM_or_PM;
    uint8_t pre_Hr = 111;
    uint8_t pre_Min = 111;
    uint8_t pre_Day = 111;
    uint8_t pre_Month = 111;

  while (THIS_SERV_IS_ACTIVE == pdTRUE){
    time(&present);
    now = localtime(&present);
  if((now->tm_year) < 124){
    //printf("Time is not correct. The year is: %d\n", now->tm_year);
  }else{
//*************************************************
  if (pre_Hr != now->tm_hour || timeRefresh){
	pre_Hr = now->tm_hour;

    if(pre_Hr == 0){
        pre_Hr = 12;
    sprintf(rtc_Hr_Min, "%d", pre_Hr );
        }
	
	else if (pre_Hr < 10){
		rtc_Hr_Min[0] = '0';
    sprintf(&rtc_Hr_Min[1], "%d", pre_Hr);
		}
	else if (pre_Hr == 10 || pre_Hr == 11){
      sprintf( rtc_Hr_Min, "%d", pre_Hr );
	}	
	else if (pre_Hr == 12){
    sprintf( rtc_Hr_Min, "%d", pre_Hr);
	}
	else if(pre_Hr > 12 && pre_Hr < 22){
		pre_Hr -= 12;
		rtc_Hr_Min[0] = '0';
    sprintf(&rtc_Hr_Min[1], "%d", pre_Hr );
		}
	else { pre_Hr -= 12;
    sprintf( rtc_Hr_Min, "%d", pre_Hr );
		}
    pre_Hr = now->tm_hour;        // Code is placed here because pre_Hr was changed.
  }

  	if (pre_Min != now->tm_min || timeRefresh){
  pre_Min = now->tm_min;

  if (pre_Min < 10){
		rtc_Hr_Min[3] = '0';
    sprintf(&rtc_Hr_Min[4], "%d", pre_Min);
		} else {
    sprintf(&rtc_Hr_Min[3], "%d", pre_Min);
	}
  rtc_Hr_Min[2] = ':';

  rtc_Hr_Min[5] = 0;
  hr_min_Obj.writeString(rtc_Hr_Min);
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
    mnth_day_Obj.writeString(rtc_Dated);
  }

  }
  if(xQueueReceive(clock_Update_Q, &clk_Updt, 0)){
  hr_min_Obj.color = clk_Updt.hourMinColour;
  timeRefresh = pdTRUE;
  } else{
    //printf("Hello Status Clock\n");
    timeRefresh = pdFALSE;
    vTaskDelay(pdMS_TO_TICKS(1000));  
  }
}

kill_This_Service(thisServ);
}






