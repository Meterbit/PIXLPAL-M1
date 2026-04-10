#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_ntp.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "gifdec.h"

static const char TAG[] = "PIXL_ANIM_CLOCK";

using namespace std;

#define HEADER_TEXT_LIMIT    13

struct PixAnimClkSettings_t{
    char headerText[200];
    uint16_t headerTextColor;
    uint16_t themeColor[2];
    uint16_t animInterval;
};

EXT_RAM_BSS_ATTR PixAnimClkSettings_t savedPixAnimClkSet;

EXT_RAM_BSS_ATTR TaskHandle_t pixAnimClock_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t pixAnimClockGif_Task_H = NULL;
void pixAnimClock_App_Task(void *);

void printPixAnimClkThm(uint16_t*);
void pixelAnimChangeButton(button_event_t);
void pixAnimClockGif_Task(void *);
void pixAnimClk_App_Task(void *);

void setClockTitleAndColor(JsonDocument&);
void setPixAnimTheme(JsonDocument&);
void setPixAnimClkColors(JsonDocument&);
void selectDisplayAnimation(JsonDocument&);
void setPixAnimInterval(JsonDocument&);
void requestNTP_Time(JsonDocument&);

Mtb_CentreText_t* headerText;
Mtb_ScrollText_t* headerTextScroll;

EXT_RAM_BSS_ATTR Mtb_Services *pixAnimClkGif_Sv = new Mtb_Services(pixAnimClockGif_Task, &pixAnimClockGif_Task_H, "Anim Clk Task", 4096);
EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *pixelAnimClock_App = new Mtb_Applications_FullScreen(pixAnimClock_App_Task, &pixAnimClock_Task_H, "Pixel Anim Clk", 4096);

void  pixAnimClock_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(pixelAnimChangeButton, mtb_Brightness_Control);

  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(setClockTitleAndColor, setPixAnimTheme, setPixAnimClkColors, requestNTP_Time);

  THIS_APP->mtb_App_Init(pixAnimClkGif_Sv);
  //************************************************************************************************************************ */

  mtb_Read_Nvs_Struct("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
  mtb_Read_Nvs_Struct("ntp TimeZone", ntp_TimeZone, sizeof(ntp_TimeZone));

  Mtb_FixedText_t hr_min_Obj(52, 26, Terminal10x17, clk_Updt.hourMinColour);
  Mtb_FixedText_t sec_Obj(102, 26, Terminal6x8, clk_Updt.secColor);
  Mtb_FixedText_t am_Pm_Obj(102, 36, Terminal6x8, clk_Updt.meridiemColor);
  Mtb_FixedText_t wkDay_Obj(45, 50, Terminal6x8, clk_Updt.weekDayColour);
  Mtb_FixedText_t date_Obj(72, 50, Terminal6x8, clk_Updt.dateColour);

  Mtb_FixedText_t awaitingNTP_Time_Line1(44, 26, Terminal6x8, LEMON);
  Mtb_FixedText_t awaitingNTP_Time_Line2(44, 36, Terminal6x8, LEMON);
  Mtb_FixedText_t awaitingNTP_Time_Line3(44, 46, Terminal6x8, LEMON);

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

  headerText = new Mtb_CentreText_t(63, 9, Terminal10x17, YELLOW);
  headerTextScroll = new Mtb_ScrollText_t(2, 1, 124, Terminal10x17, WHITE, 30, 0xFFFF, 4000);

  savedPixAnimClkSet = (PixAnimClkSettings_t){
      .headerText = "HAPPY HOME",
      .headerTextColor = BLACK,
      .themeColor = {TEAL, YELLOW},
      .animInterval = 1
  };

  mtb_Read_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));

  
  printPixAnimClkThm(savedPixAnimClkSet.themeColor);
  if(strlen(savedPixAnimClkSet.headerText) < HEADER_TEXT_LIMIT){
    headerText->mtb_Write_Colored_Text(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor, savedPixAnimClkSet.themeColor[0]);
  } else {
    headerTextScroll->backgroundColor = savedPixAnimClkSet.themeColor[0];
    headerTextScroll->mtb_Scroll_This_Text(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor);
  }

//************************************************** */
    time(&present);
    now = localtime(&present);
    if((now->tm_year) < 124){
    awaitingNTP_Time_Line1.mtb_Write_String("RETRIEVING");
    awaitingNTP_Time_Line2.mtb_Write_String("TIME FROM");
    awaitingNTP_Time_Line3.mtb_Write_String("NETWORK...");
    }
    while(((now->tm_year) < 124) && (THIS_APP_IS_ACTIVE == pdTRUE)) {
      delay(500);
      time(&present);
      now = localtime(&present);
    }
    awaitingNTP_Time_Line1.mtb_Clear_String();
    awaitingNTP_Time_Line2.mtb_Clear_String();
    awaitingNTP_Time_Line3.mtb_Clear_String();
//************************************************** */

  while (THIS_APP_IS_ACTIVE == pdTRUE){
  time(&present);
  now = localtime(&present);

  //*************************************************
  if (pre_Sec != now->tm_sec || THIS_APP->elementRefresh){
        pre_Sec = now->tm_sec;
        if (pre_Sec < 10){
            rtc_Sec[0] = '0';
            sprintf(&rtc_Sec[1], "%d", pre_Sec);
		} else {
    sprintf(rtc_Sec, "%d", pre_Sec);
	}
  rtc_Sec[2] = 0;
  sec_Obj.mtb_Write_String(rtc_Sec);
  }

  if (pre_Hr != now->tm_hour || THIS_APP->elementRefresh){
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
    am_Pm_Obj.mtb_Write_String(rtc_Am_Pm);
  }

	if (pre_Min != now->tm_min || THIS_APP->elementRefresh){
  pre_Min = now->tm_min;

  if (pre_Min < 10){
		rtc_Hr_Min[3] = '0';
    sprintf(&rtc_Hr_Min[4], "%d", pre_Min);
		} else {
    sprintf(&rtc_Hr_Min[3], "%d", pre_Min);
	}
  rtc_Hr_Min[2] = ':';

  rtc_Hr_Min[5] = 0;
  hr_min_Obj.mtb_Write_String(rtc_Hr_Min);
}
//***************************************************
if (pre_Month != now->tm_mon  || THIS_APP->elementRefresh){
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
if (pre_Year != now->tm_year  || THIS_APP->elementRefresh){
  pre_Year = now->tm_year - 100;    // subtract 2000 from the year received e.g. 2021 - 2000 = 21

	if (pre_Year < 10){
		rtc_Dated[7] = '0';
    sprintf(&rtc_Dated[8], "%d", pre_Year );
		} else {
    sprintf(&rtc_Dated[7], "%d", pre_Year);     //Review the write size/length from 14 to 2 or 3
	}
}

if(pre_Day != now->tm_mday  || THIS_APP->elementRefresh){

  pre_WeekDay = now->tm_wday;
  pre_Day = now-> tm_mday;
  
  switch (pre_WeekDay){
  case SUN: strcpy(rtc_WkDay, "SUN");
    break;
  case MON: strcpy(rtc_WkDay, "MON");
    break;
  case TUE: strcpy(rtc_WkDay, "TUE");
    break;
  case WED: strcpy(rtc_WkDay, "WED");
    break;
  case THU: strcpy(rtc_WkDay, "THU");
    break;
  case FRI: strcpy(rtc_WkDay, "FRI");
    break;
  case SAT: strcpy(rtc_WkDay, "SAT");
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
    wkDay_Obj.mtb_Write_String(rtc_WkDay);
    date_Obj.mtb_Write_String(rtc_Dated);
  }
  
  if(xQueueReceive(clock_Update_Q, &clk_Updt,0)){
  hr_min_Obj.color = clk_Updt.hourMinColour;
  sec_Obj.color = clk_Updt.secColor;
  am_Pm_Obj.color = clk_Updt.meridiemColor;
  wkDay_Obj.color = clk_Updt.weekDayColour;
  date_Obj.color = clk_Updt.dateColour;
  THIS_APP->elementRefresh = true;
  } else{
    THIS_APP->elementRefresh = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  }

  delete headerText;
  delete headerTextScroll;

  mtb_Delete_This_App(THIS_APP);
}

//**0*********************************************************************************************************************
void setClockTitleAndColor(JsonDocument& dCommand){
  const char *color = NULL;
  uint16_t titleColor = 0;
  const char *title = dCommand["clockTitle"];

  color = dCommand["color"];
  titleColor = mtb_Panel_Color32bit_To_Color565(color);

  if(strlen(title) < HEADER_TEXT_LIMIT){
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerText->mtb_Write_Colored_Text(title, titleColor, savedPixAnimClkSet.themeColor[0]);
  }else{
    headerTextScroll->backgroundColor = savedPixAnimClkSet.themeColor[0];
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerTextScroll->mtb_Scroll_This_Text(title, titleColor);
  }

  strcpy(savedPixAnimClkSet.headerText, title);
  savedPixAnimClkSet.headerTextColor = titleColor;
  mtb_Write_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
}
//**1*********************************************************************************************************************
void setPixAnimTheme(JsonDocument& dCommand){
    const char* color = NULL;
    const char* name = dCommand["name"];

    mtb_Read_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));

    if (strcmp(name, "Outer Shell") == 0){
    color = dCommand["value"];

    savedPixAnimClkSet.themeColor[0] = mtb_Panel_Color32bit_To_Color565(color);
    }
    else if (strcmp(name, "Inner Shell") == 0){
    color = dCommand["value"];

    savedPixAnimClkSet.themeColor[1] = mtb_Panel_Color32bit_To_Color565(color);
    } else color = NULL;

    printPixAnimClkThm(savedPixAnimClkSet.themeColor);

  if(strlen(savedPixAnimClkSet.headerText) < HEADER_TEXT_LIMIT){
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerText->mtb_Write_Colored_Text(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor, savedPixAnimClkSet.themeColor[0]);
  } else {
    headerTextScroll->backgroundColor = savedPixAnimClkSet.themeColor[0];
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerTextScroll->mtb_Scroll_This_Text(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor);
  }

    xQueueSend(clock_Update_Q, &clk_Updt, 0);
    mtb_Write_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
}
//**2*********************************************************************************************************************
void setPixAnimClkColors(JsonDocument& dCommand){
  const char *color = NULL;
  const char *name = dCommand["name"];
  Clock_Colors clk_Cols;

  mtb_Read_Nvs_Struct("Clock Cols", &clk_Cols, sizeof(Clock_Colors));

  if (strcmp(name, "Hour/Minute") == 0)
  {
    color = dCommand["value"];
    

    clk_Cols.hourMinColour = mtb_Panel_Color32bit_To_Color565(color);
    }
    else if (strcmp(name, "Seconds") == 0){
    color = dCommand["value"];

    clk_Cols.secColor = mtb_Panel_Color32bit_To_Color565(color);
    }
    else if (strcmp(name, "Meridiem") == 0){
    color = dCommand["value"];

    clk_Cols.meridiemColor = mtb_Panel_Color32bit_To_Color565(color);
    }
    else if (strcmp(name, "Weekday") == 0){
    color = dCommand["value"];

    clk_Cols.weekDayColour = mtb_Panel_Color32bit_To_Color565(color);
    }
    else if (strcmp(name, "Date") == 0){
    color = dCommand["value"];

    clk_Cols.dateColour = mtb_Panel_Color32bit_To_Color565(color);
    } else color = NULL;

    mtb_Write_Nvs_Struct("Clock Cols", &clk_Cols, sizeof(Clock_Colors));
    xQueueSend(clock_Update_Q, &clk_Cols, 0);
}

void requestNTP_Time(JsonDocument& dCommand){
  mtb_Launch_This_Service(mtb_Sntp_Time_Sv);
}

void pixelAnimChangeButton(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            //ESP_LOGI(TAG, "Button Released\n");
            break;

            case BUTTON_PRESSED:
              
              break;

            case BUTTON_PRESSED_LONG:
              mtb_Launch_This_App(calendarClock_App);
              break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}


void pixAnimClockGif_Task(void* d_Service){	// Consider using hardware timer for updating this function to save processor space/time.
	Mtb_Services *thisServ = (Mtb_Services *)d_Service;
  uint16_t colorHolder = 0xFF;

  while (MTB_SERV_IS_ACTIVE == pdTRUE){
DIR *dir = opendir("/littlefs/clkgif");
if (!dir) {
  ESP_LOGE(TAG, "Failed to open /littlefs/clkgif");
  vTaskDelay(pdMS_TO_TICKS(1000));
  continue;
}

struct dirent *entry;
while (MTB_SERV_IS_ACTIVE == pdTRUE && (entry = readdir(dir)) != NULL) {
  // Skip "." ".." and hidden/metadata files like .DS_Store
  if (entry->d_name[0] == '.') continue;

  // Only accept *.gif (case-insensitive)
  const char *name = entry->d_name;
  size_t n = strlen(name);
  if (n < 4) continue;
  const char *ext = name + (n - 4);
  if (strcasecmp(ext, ".gif") != 0) continue;

  char fullPath[300];
  snprintf(fullPath, sizeof(fullPath), "/littlefs/clkgif/%s", name);
  ESP_LOGI(TAG, "GIF File: %s", fullPath);

  gd_GIF *gif = gd_open_gif(fullPath);
  if (!gif) {
    ESP_LOGW(TAG, "gd_open_gif failed: %s", fullPath);
    continue;
  }

  uint16_t width = gif->width;
  uint16_t height = gif->height;
  if (width >= 65 || height >= 33) {
    gd_close_gif(gif);
    continue;
  }

  uint8_t *buffer = (uint8_t *)malloc(width * height * 3);
  if (!buffer) {
    ESP_LOGE(TAG, "malloc failed (%ux%u)", width, height);
    gd_close_gif(gif);
    continue;
  }

  uint32_t set_Duration = 60000;
  for (uint32_t show_Duration = 0;
       MTB_SERV_IS_ACTIVE == pdTRUE && show_Duration < set_Duration; ) {
    while (MTB_SERV_IS_ACTIVE == pdTRUE && gd_get_frame(gif)) {
      gd_render_frame(gif, buffer);

      mtb_Panel_Draw_FrameRGB888(5, 25, width, height, buffer);

      TickType_t delay_ms = gif->gce.delay * 10;
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
      show_Duration += delay_ms;
    }
    gd_rewind(gif);
  }

  free(buffer);
  gd_close_gif(gif);
}
closedir(dir);
	}
  mtb_Delete_This_Service(thisServ);
}

void printPixAnimClkThm(uint16_t* themeColor){
  mtb_Panel_Fill_Screen(themeColor[0]);   // Fill background color1.
  mtb_Panel_Draw_Rect(1, 19, 126, 62, BLACK);   // Draw outer border.
  mtb_Panel_Fill_Rect(2, 20, 125, 61, themeColor[1]);   // Fill background color2
  mtb_Panel_Fill_Rect(43, 23, 122, 58, BLACK);           // Fill clock area color.
  mtb_Panel_Draw_Rect(4, 24, 37, 57, BLACK);   // Draw inner borders1
}