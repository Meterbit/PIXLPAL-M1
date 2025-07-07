#ifndef NVS_MEM
#define NVS_MEM

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mtbApps.h"

  struct Wifi_Credentials {
      char ssid[32];
      char pass[64];
      char ipAdr[20];
  };

  // APPLICATIONS DEFAULT SETTINGS *******************************************************************************************    
  struct Clock_Colors{
      uint16_t hourMinColour;
      uint16_t secColor;
      uint16_t meridiemColor;
      uint16_t weekDayColour;
      uint16_t dateColour;
  };

//*********************************************************************************************************** */
    extern char serial_No[20];
    extern char pxp_BLE_Name[20];
    extern char ntp_TimeZone[100];
    extern uint8_t deviceVolume;
    extern uint8_t litFS_Ready;
    extern Clock_Colors clk_Updt;
    extern void init_nvs_mem(void);
    void set_factory_NVS_parameters(void);

#endif