#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_ntp.h"
#include "mtb_engine.h"
#include "mtb_bigClock.h"

void bigClock_Color_Change(JsonDocument&);
void bigClockGet_NTP_Local_Time(JsonDocument&);

EXT_RAM_BSS_ATTR TaskHandle_t bigClockCalendar_Task_H = NULL;

EXT_RAM_BSS_ATTR Applications_FullScreen *bigClockCalendar_App = new Applications_FullScreen(bigClock_App_Task, &bigClockCalendar_Task_H, "big Clock", 10240, pdTRUE);
//***************************************************************************************************
void  bigClock_App_Task(void* dApplication){
  Applications *thisApp = (Applications *)dApplication;
  thisApp->app_EncoderFn_ptr = brightnessControl;
  thisApp->app_ButtonFn_ptr = randomButtonControl;
  ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(bigClock_Color_Change, bigClockGet_NTP_Local_Time);
  appsInitialization(thisApp);
  //**************************************************************************************************************************
  read_struct_from_nvs("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
  FixedText_t hr_min_Obj(0, 13, PT_Sans_Narrow38x34, clk_Updt.hourMinColour);
  FixedText_t sec_Obj(105, 12, Terminal10x16, clk_Updt.secColor);
  FixedText_t am_Pm_Obj(105, 32, Terminal10x16, clk_Updt.meridiemColor);
  FixedText_t wkDay_Obj(2, 55, Terminal6x8, clk_Updt.weekDayColour);
  FixedText_t date_Obj(77, 55, Terminal6x8, clk_Updt.dateColour);

  FixedText_t awaitingNTP_Time_Top(10, 25, Terminal8x12, LEMON);
  FixedText_t awaitingNTP_Time_But(10, 38, Terminal8x12, LEMON);

  char rtc_Hr_Min[10] = {0};
  char rtc_Sec[10] = {0};
  char rtc_Am_Pm[10] = {0};
  char rtc_WkDay[15] = {0};
  char rtc_Dated[25] = {0};
  time_t present = 0;
  struct tm *now = nullptr;
  char AM_or_PM;
  uint8_t pre_Sec = 111; // 111 is an abitrary number choosen which is greater than 59 seconds but less than 256 count of 8bytes
  uint8_t pre_Hr = 111;
  uint8_t pre_Min = 111;
  uint8_t pre_WeekDay;
  uint8_t pre_Day = 111;
  uint8_t pre_Month = 111;
  uint16_t pre_Year = 111;

//************************************************** */
    time(&present);
    now = localtime(&present);
    if((now->tm_year) < 124){
      awaitingNTP_Time_Top.writeString("RETRIEVING TIME");
      awaitingNTP_Time_But.writeString("FROM NETWORK...");
    }
    while((now->tm_year) < 124 && THIS_APP_IS_ACTIVE) {
      delay(1000);
      time(&present);
      now = localtime(&present);
    }
    awaitingNTP_Time_Top.clearString();
    awaitingNTP_Time_But.clearString();
//************************************************** */
  while (THIS_APP_IS_ACTIVE == pdTRUE){
    time(&present);
    now = localtime(&present);

  //*************************************************
  if (pre_Sec != now->tm_sec || thisApp->elementRefresh){
        pre_Sec = now->tm_sec;
        if (pre_Sec < 10){
            rtc_Sec[0] = '0';
            sprintf(&rtc_Sec[1], "%d", pre_Sec);
		} else {
    sprintf(rtc_Sec, "%d", pre_Sec);
	}
  rtc_Sec[2] = 0;
  sec_Obj.writeString(rtc_Sec);
  }

  if (pre_Hr != now->tm_hour || thisApp->elementRefresh){
	pre_Hr = now->tm_hour;

  if(pre_Hr == 0){
	pre_Hr = 12;
  sprintf( rtc_Hr_Min, "%d", pre_Hr );
	AM_or_PM = 'A';
	}
	
	else if (pre_Hr < 10){
		rtc_Hr_Min[0] = '0';
    sprintf(&rtc_Hr_Min[1], "%d", pre_Hr);
		AM_or_PM = 'A';
		}
	else if (pre_Hr == 10 || pre_Hr == 11){
      sprintf( rtc_Hr_Min, "%d", pre_Hr );
		AM_or_PM = 'A';
	}	
	else if (pre_Hr == 12){
    sprintf( rtc_Hr_Min, "%d", pre_Hr);
		AM_or_PM = 'P';
	}
	else if(pre_Hr > 12 && pre_Hr < 22){
		pre_Hr -= 12;
		rtc_Hr_Min[0] = '0';
    sprintf(&rtc_Hr_Min[1], "%d", pre_Hr );
		AM_or_PM = 'P';
		}
	else { pre_Hr -= 12;
    sprintf( rtc_Hr_Min, "%d", pre_Hr );
		AM_or_PM = 'P';
		}

    pre_Hr = now->tm_hour;        // Code is placed here because pre_Hr was changed.

    rtc_Am_Pm[0] = AM_or_PM;
    rtc_Am_Pm[1] = 'M';
    rtc_Am_Pm[2] = 0;
    am_Pm_Obj.writeString(rtc_Am_Pm);
  }

	if (pre_Min != now->tm_min || thisApp->elementRefresh){
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
//***************************************************
if (pre_Month != now->tm_mon  || thisApp->elementRefresh){
pre_Month = now->tm_mon;

switch (pre_Month){
      case JANUARY: strcpy(&rtc_Dated[3], "JAN");
      break;
      case FEBUARY: strcpy(&rtc_Dated[3], "FEB");
      break;
      case MARCH: strcpy(&rtc_Dated[3], "MAR");
      break;
      case APRIL: strcpy(&rtc_Dated[3], "APR");
      break;
      case MAY: strcpy(&rtc_Dated[3], "MAY");
      break;
      case JUNE: strcpy(&rtc_Dated[3], "JUN");
      break;
      case JULY: strcpy(&rtc_Dated[3], "JUL");
      break;
      case AUGUST: strcpy(&rtc_Dated[3], "AUG");
      break;
      case SEPTEMBER: strcpy(&rtc_Dated[3], "SEP");
      break;
      case OCTOBER: strcpy(&rtc_Dated[3], "OCT");
      break;
      case NOVEMBER: strcpy(&rtc_Dated[3], "NOV");
      break;
      case DECEMBER: strcpy(&rtc_Dated[3], "DEC");
      break;
      default: strcpy(&rtc_Dated[3], "ERR");
}
}

//************************************************************
if (pre_Year != now->tm_year  || thisApp->elementRefresh){
  pre_Year = now->tm_year - 100;    // subtract 2000 from the year received e.g. 2021 - 2000 = 21

	if (pre_Year < 10){
		rtc_Dated[7] = '0';
    sprintf(&rtc_Dated[8], "%d", pre_Year );
		} else {
    sprintf(&rtc_Dated[7], "%d", pre_Year);     //Review the write size/length from 14 to 2 or 3
	}
}

if(pre_Day != now->tm_mday  || thisApp->elementRefresh){

  pre_WeekDay = now->tm_wday;
  pre_Day = now-> tm_mday;
  
  switch (pre_WeekDay){
  case SUN: strcpy(rtc_WkDay, "SUNDAY");
    break;
  case MON: strcpy(rtc_WkDay, "MONDAY");
    break;
  case TUE: strcpy(rtc_WkDay, "TUESDAY");
    break;
  case WED: strcpy(rtc_WkDay, "WEDNESDAY");
    break;
  case THU: strcpy(rtc_WkDay, "THURSDAY");
    break;
  case FRI: strcpy(rtc_WkDay, "FRIDAY");
    break;
  case SAT: strcpy(rtc_WkDay, "SATURDAY");
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
    wkDay_Obj.writeString(rtc_WkDay);
    date_Obj.writeString(rtc_Dated);
  }
  
  if(xQueueReceive(clock_Update_Q, &clk_Updt,0)){
  hr_min_Obj.color = clk_Updt.hourMinColour;
  sec_Obj.color = clk_Updt.secColor;
  am_Pm_Obj.color = clk_Updt.meridiemColor;
  wkDay_Obj.color = clk_Updt.weekDayColour;
  date_Obj.color = clk_Updt.dateColour;
  thisApp->elementRefresh = true;
  } else{
    thisApp->elementRefresh = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  }
  kill_This_App(thisApp);
}

//**13*********************************************************************************************************************
void showbigClockCalendar(JsonDocument& dCommand){ //use the radio button selection widget
  uint8_t cmd = dCommand["app_command"];
  //start_This_Service(sntp_Time_Sv);
  mtb_Ble_App_Cmd_Respond_Success(bigClockCalendarAppRoute, cmd, pdPASS);
}

//**11*********************************************************************************************************************
void bigClock_Color_Change(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  const char *color = NULL;
  const char *name = dCommand["name"];
  Clock_Colors clk_Cols;

  read_struct_from_nvs("Clock Cols", &clk_Cols, sizeof(Clock_Colors));

  if (strcmp(name, "Hour/Minute") == 0)
  {
    color = dCommand["value"];
    color += 4;

    clk_Cols.hourMinColour = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Seconds") == 0){
    color = dCommand["value"];
    color += 4;
    clk_Cols.secColor = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Meridiem") == 0){
    color = dCommand["value"];
    color += 4;
    clk_Cols.meridiemColor = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Weekday") == 0){
    color = dCommand["value"];
    color += 4;
    clk_Cols.weekDayColour = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Date") == 0){
    color = dCommand["value"];
    color += 4;
    clk_Cols.dateColour = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    } else color = NULL;

    write_struct_to_nvs("Clock Cols", &clk_Cols, sizeof(Clock_Colors));
    xQueueSend(clock_Update_Q, &clk_Cols, 0);
    mtb_Ble_App_Cmd_Respond_Success(bigClockCalendarAppRoute, cmd, pdPASS);
}
//**12*********************************************************************************************************************

//**13*********************************************************************************************************************
void bigClockGet_NTP_Local_Time(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  start_This_Service(sntp_Time_Sv);
  mtb_Ble_App_Cmd_Respond_Success(bigClockCalendarAppRoute, cmd, pdPASS);
}