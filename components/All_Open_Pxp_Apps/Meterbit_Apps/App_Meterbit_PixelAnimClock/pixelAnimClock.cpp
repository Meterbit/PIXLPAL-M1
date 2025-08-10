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
#include "mtb_cal_clk.h"
#include "pixelAnimClock.h"
#include "mtb_text_scroll.h"
#include "gifdec.h"

using namespace std;

#define HEADER_TEXT_LIMIT    13

PixAnimClkSettings_t savedPixAnimClkSet{
    .headerText = "HAPPY HOME",
    .headerTextColor = BLACK,
    .themeColor = {TEAL, YELLOW, BLACK},
    .animInterval = 1
};

//uint8_t set_Pixel_Change_Duration = 1;
EXT_RAM_BSS_ATTR TaskHandle_t pixAnimClock_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t pixAnimClockGif_Task_H = NULL;
void printPixAnimClkThm(uint16_t*);
void pixelAnimChangeButton(button_event_t);
void pixAnimClockGif_Task(void *d_Arguments);

void setClockTitleAndColor(JsonDocument&);
void setPixAnimTheme(JsonDocument&);
void setPixAnimClkColors(JsonDocument&);
void selectDisplayAnimation(JsonDocument&);
void setPixAnimInterval(JsonDocument&);
void requestNTP_Time(JsonDocument&);

Mtb_CentreText_t* headerText;
Mtb_ScrollText_t* headerTextScroll;

EXT_RAM_BSS_ATTR Mtb_Services *pixAnimClkGif_Sv = new Mtb_Services(pixAnimClockGif_Task, &pixAnimClockGif_Task_H, "Anim Clk Task", 4096); // THIS APP CANNOT BE IN PSRAM BECAUSE IT WRITES/READS THE FLASH DURING OPERATION.
EXT_RAM_BSS_ATTR Applications_FullScreen *pixelAnimClock_App = new Applications_FullScreen(pixAnimClock_App_Task, &pixAnimClock_Task_H, "Pixel Anim Clk", 4096, pdTRUE); // Review down this stack size later.

void  pixAnimClock_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = pixelAnimChangeButton;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setClockTitleAndColor, setPixAnimTheme, setPixAnimClkColors, requestNTP_Time);
  mtb_App_Init(thisApp, pixAnimClkGif_Sv);
  //************************************************************************************************************************ */
  mtb_Read_Nvs_Struct("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
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
  headerTextScroll = new Mtb_ScrollText_t(2, 1, 124, WHITE, 30, 0xFFFF, Terminal10x17, 4000);


  mtb_Read_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
  printPixAnimClkThm(savedPixAnimClkSet.themeColor);
  if(strlen(savedPixAnimClkSet.headerText) < HEADER_TEXT_LIMIT){
    headerText->mtb_Write_Colored_String(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor, savedPixAnimClkSet.themeColor[0]);
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
    while(((now->tm_year) < 124) && (MTB_APP_IS_ACTIVE == pdTRUE)) {
      delay(500);
      time(&present);
      now = localtime(&present);
    }
    awaitingNTP_Time_Line1.clearString();
    awaitingNTP_Time_Line2.clearString();
    awaitingNTP_Time_Line3.clearString();
//************************************************** */

  while (MTB_APP_IS_ACTIVE == pdTRUE){
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
  sec_Obj.mtb_Write_String(rtc_Sec);
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
    am_Pm_Obj.mtb_Write_String(rtc_Am_Pm);
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
  hr_min_Obj.mtb_Write_String(rtc_Hr_Min);
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
  thisApp->elementRefresh = true;
  } else{
    thisApp->elementRefresh = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  }

  delete headerText;
  delete headerTextScroll;

  mtb_End_This_App(thisApp);
}

//**0*********************************************************************************************************************
void setClockTitleAndColor(JsonDocument& dCommand){
  String success = "{\"pxp_command\":0, \"response\": 1}";
  const char *color = NULL;
  uint16_t titleColor = 0;
  const char *title = dCommand["clockTitle"];

  color = dCommand["color"];
  color += 4;
  titleColor = dma_display->color565(((uint8_t)((strtol(color, NULL, 16) >> 16))), ((uint8_t)((strtol(color, NULL, 16) >> 8))), ((uint8_t)((strtol(color, NULL, 16) >> 0))));

  if(strlen(title) < HEADER_TEXT_LIMIT){
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerText->mtb_Write_Colored_String(title, titleColor, savedPixAnimClkSet.themeColor[0]);
  }else{
    headerTextScroll->backgroundColor = savedPixAnimClkSet.themeColor[0];
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerTextScroll->mtb_Scroll_This_Text(title, titleColor);
  }

  strcpy(savedPixAnimClkSet.headerText, title);
  savedPixAnimClkSet.headerTextColor = titleColor;
  mtb_Write_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
  bleApplicationComSend(pixelAnimClockAppRoute, success);
}
//**1*********************************************************************************************************************
void setPixAnimTheme(JsonDocument& dCommand){
    String success = "{\"pxp_command\": 1, \"response\": 1}";
    const char* color = NULL;
    const char* name = dCommand["name"];

    mtb_Read_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));

    if (strcmp(name, "Outer Shell") == 0){
    color = dCommand["value"];
    color += 4;

    savedPixAnimClkSet.themeColor[0] = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Inner Shell") == 0){
    color = dCommand["value"];
    color += 4;
    savedPixAnimClkSet.themeColor[1] = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    }
    else if (strcmp(name, "Borders") == 0){
    color = dCommand["value"];
    color += 4;
    
    //savedPixAnimClkSet.themeColor[2] = dma_display->color565(((uint8_t)((strtol(color,NULL,16) >> 16))), ((uint8_t)((strtol(color,NULL,16) >> 8))),((uint8_t)((strtol(color,NULL,16) >> 0))));
    savedPixAnimClkSet.themeColor[2] = BLACK;
    } else color = NULL;

    printPixAnimClkThm(savedPixAnimClkSet.themeColor);

  if(strlen(savedPixAnimClkSet.headerText) < HEADER_TEXT_LIMIT){
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerText->mtb_Write_Colored_String(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor, savedPixAnimClkSet.themeColor[0]);
  } else {
    headerTextScroll->backgroundColor = savedPixAnimClkSet.themeColor[0];
    headerTextScroll->mtb_Scroll_Active(STOP_SCROLL);
    headerTextScroll->mtb_Scroll_This_Text(savedPixAnimClkSet.headerText, savedPixAnimClkSet.headerTextColor);
  }

    xQueueSend(clock_Update_Q, &clk_Updt, 0);
    mtb_Write_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
    bleApplicationComSend(pixelAnimClockAppRoute, success);
}
//**2*********************************************************************************************************************
void setPixAnimClkColors(JsonDocument& dCommand){
  uint8_t cmdNumber = dCommand["app_command"];
  const char *color = NULL;
  const char *name = dCommand["name"];
  Clock_Colors clk_Cols;

  mtb_Read_Nvs_Struct("Clock Cols", &clk_Cols, sizeof(Clock_Colors));

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

    mtb_Write_Nvs_Struct("Clock Cols", &clk_Cols, sizeof(Clock_Colors));
    xQueueSend(clock_Update_Q, &clk_Cols, 0);
    mtb_Ble_App_Cmd_Respond_Success(pixelAnimClockAppRoute, cmdNumber, pdPASS);
}
//**3********************************************************************************************************************************************************
// void selectDisplayAnimation(JsonDocument& dCommand){
//   char success[] = "{\"pxp_command\": 3, \"response\": 1}";
//   uint8_t direction = dCommand["direction"];
//   if(direction) ESP_LOGI(TAG, "Right Hand Direction Selected");
//   else ESP_LOGI(TAG, "Left Hand Direction Selected");
//   mqttServer.publish(apps_mqtt_data.topic_Response, success);
// }
// //**4********************************************************************************************************************************************************
// void setPixAnimInterval(JsonDocument& dCommand){
//   char success[] = "{\"pxp_command\": 4, \"response\": 1}";
//   uint animIntervalHolder = dCommand["value"];
//   mtb_Read_Nvs_Struct("pixAnimClk", &savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
//   savedPixAnimClkSet.animInterval = animIntervalHolder;
//   mtb_Write_Nvs_Struct("pixAnimClk",&savedPixAnimClkSet, sizeof(PixAnimClkSettings_t));
//   mqttServer.publish(apps_mqtt_data.topic_Response, success);
// }
//**5*********************************************************************************************************************
void requestNTP_Time(JsonDocument& dCommand){
  uint8_t cmdNumber = dCommand["app_command"];
  mtb_Start_This_Service(sntp_Time_Sv);
  mtb_Ble_App_Cmd_Respond_Success(pixelAnimClockAppRoute, cmdNumber, pdPASS);
}

void pixelAnimChangeButton(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            //ESP_LOGI(TAG, "Button Released\n");
            break;

            case BUTTON_PRESSED:
              
              break;

            case BUTTON_PRESSED_LONG:
              mtb_Launch_This_App(classicClock_App);
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
		struct dirent* entry;

		while (MTB_SERV_IS_ACTIVE == pdTRUE && (entry = readdir(dir)) != NULL){
			char fullPath[300];
			uint16_t width, height;
			sprintf(fullPath, "/littlefs/clkgif/%s", entry->d_name);
	
			gd_GIF *gif = gd_open_gif(fullPath);
			width = gif->width;
			height = gif->height;

		if (width < 65 && height < 33){
				uint32_t set_Duration = 60000;
				uint8_t *buffer = (uint8_t *) malloc(width * height * 3);         // thisServ->serv_Dyn_Mem_Alloc(width * height * 3);
        
			for (uint32_t show_Duration = 0; MTB_SERV_IS_ACTIVE == pdTRUE && show_Duration < set_Duration; ){
				while (MTB_SERV_IS_ACTIVE == pdTRUE && gd_get_frame(gif)){
					
					gd_render_frame(gif, buffer);

					for (uint8_t p = 0, x = 5; p < width; p++, x++){
						for (uint8_t q = 0, y = 25; q < height; q++, y++){
                colorHolder = dma_display->color565(buffer[q*width*3 + p*3], buffer[q*width*3 + p*3 + 1], buffer[q*width*3 + p*3 + 2]); 
                dma_display->drawPixel(x, y, colorHolder);    	// update color
						}
					}
					vTaskDelay((gif->gce.delay * 10) / portTICK_PERIOD_MS);
					show_Duration += gif->gce.delay * 10;
				}
				gd_rewind(gif);
		}
		free(buffer);
		}
		gd_close_gif(gif);
		}
    closedir(dir);
	}
  mtb_End_This_Service(thisServ);
}

void printPixAnimClkThm(uint16_t* themeColor){
  dma_display->fillScreen(themeColor[0]);   // Fill background color1.
  dma_display->drawRect(1, 19, 126, 44, themeColor[2]);   // Draw outer border.
  dma_display->fillRect(2, 20, 124, 42, themeColor[1]);   // Fill background color2
  dma_display->fillRect(43, 23, 80, 36, BLACK);           // Fill clock area color.

  dma_display->drawRect(4, 24, 34, 34, themeColor[2]);   // Draw inner borders1
  dma_display->drawRect(42, 22, 82, 38, themeColor[2]);   // Draw inner borders2
}