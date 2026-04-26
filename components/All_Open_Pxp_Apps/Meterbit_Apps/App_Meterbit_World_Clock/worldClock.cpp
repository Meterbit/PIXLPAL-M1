#include <Arduino.h>
#include <HTTPClient.h>
#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "workerWorldFlagsFns.h"
#include "mtb_buzzer.h"
#include "mtb_PosixTZtoLocalTime.h"

#define SINGLE_CLOCK_MODE   0
#define FIVE_CLOCK_MODE     1

struct WorldClock_Data_t {
char worldCapitals[5][50];
char worldTimeZones[5][50];
char firstCountryName[50];
uint16_t worldColors[5];
uint8_t worldClockMode;
};

EXT_RAM_BSS_ATTR TaskHandle_t worldClock_Task_H = NULL;
EXT_RAM_BSS_ATTR WorldClock_Data_t worldClockCities;
void worldClock_App_Task(void *);

// button and encoder functions
void change_City_Button(button_event_t button_Data);

// bluetooth functions
void setWorldClockCities(JsonDocument&);
void setWorldClockColors(JsonDocument&);
void setWorldClockMode(JsonDocument&);
void requestWorldClkNTP_Time(JsonDocument&);

// Helper Functions
void drawWorldClock5CitiesBkgd(void);
void drawWorldClockSingleCity(void);

// Declare the World Clock App in Status Bar Mode
EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *worldClock_App = new Mtb_Applications_StatusBar(worldClock_App_Task, &worldClock_Task_H, "world Clock", 3072, WEEKDAY_STATUS_BAR);

void worldClock_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(change_City_Button, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Sv_Fns(setWorldClockCities, setWorldClockColors, setWorldClockMode, requestWorldClkNTP_Time);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */
    char worldCity_Hr_Min[10] = {0};

    worldClockCities = (WorldClock_Data_t){
    {"New York", "London", "Tokyo", "Sydney", "Moscow"},
    {
    "EST5EDT,M3.2.0/2,M11.1.0/2",
    "GMT0BST,M3.5.0/1,M10.5.0/2",
    "JST-9",
    "AEST-10AEDT,M10.1.0/2,M4.1.0/3",
    "MSK-3"
    },
    "United States of America",
    {GREEN, WHITE, YELLOW, CYAN, MAGENTA},
    0
    };

    Mtb_FixedText_t dispSingleCityTime(70, 21, Terminal10x17, GREEN);
    Mtb_CentreText_t dispSingleCityName(65, 57, Terminal8x12, WHITE);

    Mtb_FixedText_t cityMultiName0(3, 12, Terminal6x8, GREEN_LIZARD);
    Mtb_FixedText_t cityMultiTime0(100, 12, Terminal6x8, WHITE);

    Mtb_FixedText_t cityMultiName1(3, 23, Terminal6x8, GREEN_LIZARD);
    Mtb_FixedText_t cityMultiTime1(100, 23, Terminal6x8, WHITE);

    Mtb_FixedText_t cityMultiName2(3, 34, Terminal6x8, GREEN_LIZARD);
    Mtb_FixedText_t cityMultiTime2(100, 34, Terminal6x8, WHITE);

    Mtb_FixedText_t cityMultiName3(3, 45, Terminal6x8, GREEN_LIZARD);
    Mtb_FixedText_t cityMultiTime3(100, 45, Terminal6x8, WHITE);

    Mtb_FixedText_t cityMultiName4(3, 56, Terminal6x8, GREEN_LIZARD);
    Mtb_FixedText_t cityMultiTime4(100, 56, Terminal6x8, WHITE);

    Mtb_OnlineImage_t worldCountryFlag{
      "https://raw.githubusercontent.com/woble/flags/refs/heads/master/SVG/3x2/ng.svg",
      3,
      14,
      2
    };

  mtb_Read_Nvs_Struct("worldClockNv", &worldClockCities, sizeof(WorldClock_Data_t));

if(worldClockCities.worldClockMode == FIVE_CLOCK_MODE) drawWorldClock5CitiesBkgd();
else drawWorldClockSingleCity();

while (THIS_APP_IS_ACTIVE == pdTRUE){
    THIS_APP->elementRefresh = false;
    if(worldClockCities.worldClockMode == FIVE_CLOCK_MODE){
                                                                                                                // MEMORY LEAKING OBSERVED IN THIS LOOP - NEEDS FIXING LATER
      cityMultiName0.mtb_Write_Colored_Text(worldClockCities.worldCapitals[0], worldClockCities.worldColors[0]);
      cityMultiName1.mtb_Write_Colored_Text(worldClockCities.worldCapitals[1], worldClockCities.worldColors[1]);
      cityMultiName2.mtb_Write_Colored_Text(worldClockCities.worldCapitals[2], worldClockCities.worldColors[2]);
      cityMultiName3.mtb_Write_Colored_Text(worldClockCities.worldCapitals[3], worldClockCities.worldColors[3]);
      cityMultiName4.mtb_Write_Colored_Text(worldClockCities.worldCapitals[4], worldClockCities.worldColors[4]);

      while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

    // Cache timezones at startup
      cacheTimezone(worldClockCities.worldTimeZones[0]);
      cacheTimezone(worldClockCities.worldTimeZones[1]);
      cacheTimezone(worldClockCities.worldTimeZones[2]);
      cacheTimezone(worldClockCities.worldTimeZones[3]);
      cacheTimezone(worldClockCities.worldTimeZones[4]);

      while (THIS_APP_IS_ACTIVE == pdTRUE && THIS_APP->elementRefresh == false) {
        cityMultiTime0.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[0], worldCity_Hr_Min), worldClockCities.worldColors[0]);
        cityMultiTime1.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[1], worldCity_Hr_Min), worldClockCities.worldColors[1]);
        cityMultiTime2.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[2], worldCity_Hr_Min), worldClockCities.worldColors[2]);
        cityMultiTime3.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[3], worldCity_Hr_Min), worldClockCities.worldColors[3]);
        cityMultiTime4.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[4], worldCity_Hr_Min), worldClockCities.worldColors[4]);
        delay(1000);
      }
    } else {
      
      dispSingleCityName.mtb_Write_Colored_Text(worldClockCities.worldCapitals[0], WHITE);

      while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);
      
      strcpy(worldCountryFlag.imageLink, getFlag4x3ByCountry(worldClockCities.firstCountryName).c_str());

      mtb_Draw_Online_Svg(&worldCountryFlag);       // IF FLAG IS NOT DRAWN, IT MEANS THE NAME OF THE COUNTRY WAS NOT FOUND AMONG THE COUNTRIES FLAG LISTS/JSON.

      cacheTimezone(worldClockCities.worldTimeZones[0]);

      while (THIS_APP_IS_ACTIVE == pdTRUE && THIS_APP->elementRefresh == false) {
        dispSingleCityTime.mtb_Write_Colored_Text(getCityLocalTime(worldClockCities.worldTimeZones[0], worldCity_Hr_Min), worldClockCities.worldColors[0]);
        delay(1000);
      }
    }
}

  mtb_Delete_This_App(THIS_APP);
}

void change_City_Button(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
            //mtb_Ble_Comm_Init();
            break;

            case BUTTON_PRESSED_LONG:
            //mtb_Launch_This_App(pixelAnimClock_App);
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

void drawWorldClock5CitiesBkgd(void){
    mtb_Panel_Fill_Rect(0, 10, 127, 63, BLACK);   
    uint16_t clockDividerColor = mtb_Panel_Color565(35, 35, 35);
    mtb_Panel_Draw_HLine(20, 0, 128, clockDividerColor);
    mtb_Panel_Draw_HLine(31, 0, 128, clockDividerColor);
    mtb_Panel_Draw_HLine(42, 0, 128, clockDividerColor);
    mtb_Panel_Draw_HLine(53, 0, 128, clockDividerColor);
}

void drawWorldClockSingleCity(void){
    mtb_Panel_Fill_Rect(0, 10, 127, 63, BLACK);
    mtb_Draw_Local_Png({"/batIcons/worldClk.png", 59, 14});
}

void setWorldClockCities(JsonDocument& dCommand){
  uint8_t dCityIndex = dCommand["dCityIndex"].as<uint8_t>();
  String dCityName = dCommand["dCityName"].as<String>();
  String dTimeZone = dCommand["dTimeZone"].as<String>();

  dCityName.replace("[", "");
  dCityName.replace("]", "");

  dTimeZone.replace("[", "");
  dTimeZone.replace("]", "");

  int commaIndex = dCityName.indexOf(',');

  if (commaIndex != -1 && dCityIndex == 0) {
    String country = "";
    country = dCityName.substring(commaIndex + 1); // get text after the comma
    country.trim();  // remove leading/trailing spaces
    strcpy(worldClockCities.firstCountryName, country.c_str());
  }

  if (commaIndex != -1) {
    dCityName.remove(commaIndex);   // Removes from the comma to the end
  }

  strcpy(worldClockCities.worldCapitals[dCityIndex], dCityName.c_str());
  strcpy(worldClockCities.worldTimeZones[dCityIndex], dTimeZone.c_str());

  mtb_Write_Nvs_Struct("worldClockNv", &worldClockCities, sizeof(WorldClock_Data_t));

  clearTimezoneCache();

  Mtb_Applications::currentRunningApp->elementRefresh = true;
}

void setWorldClockColors(JsonDocument& dCommand){
    const char *color = NULL;

    uint8_t dCityIndex = dCommand["dCityIndex"].as<uint8_t>();

    color = dCommand["value"];
    worldClockCities.worldColors[dCityIndex] = mtb_Panel_Color32bit_To_Color565(color);
    
    mtb_Write_Nvs_Struct("worldClockNv", &worldClockCities, sizeof(WorldClock_Data_t));
    Mtb_Applications::currentRunningApp->elementRefresh = true;
}

void setWorldClockMode(JsonDocument&){
    worldClockCities.worldClockMode = dCommand["ClockMode"].as<uint8_t>();

    if(worldClockCities.worldClockMode == FIVE_CLOCK_MODE) drawWorldClock5CitiesBkgd();
    else drawWorldClockSingleCity();

    mtb_Write_Nvs_Struct("worldClockNv", &worldClockCities, sizeof(WorldClock_Data_t));
    Mtb_Applications::currentRunningApp->elementRefresh = true;
}

void requestWorldClkNTP_Time(JsonDocument&){
    //String location = dCommand["duration"];
    Mtb_Applications::currentRunningApp->elementRefresh = true;
}