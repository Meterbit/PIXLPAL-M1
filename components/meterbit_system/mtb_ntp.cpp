#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sntp.h"
#include "mtb_ntp.h"
#include "mtb_nvs.h"
#include "mtb_ota.h"
#include "mtb_engine.h"
#include "mtb_buzzer.h"

EXT_RAM_BSS_ATTR TaskHandle_t sntp_Time_handle = NULL;
EXT_RAM_BSS_ATTR Mtb_Services *sntp_Time_Sv = new Mtb_Services(sntp_Time_init_Task, &sntp_Time_handle, "NTP Init", 4096);

void on_got_time(struct timeval* tv){
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  mtb_Read_Nvs_Struct("Clock Cols", &clk_Updt, sizeof(Clock_Colors));
  xQueueSendFromISR(clock_Update_Q, &clk_Updt, &xHigherPriorityTaskWoken);
  mtb_Launch_This_App(otaUpdateApplication_App, IGNORE_PREVIOUS_APP);
}

void sntp_Time_init_Task(void* dService){
  Mtb_Services *thisService = (Mtb_Services *)dService;
  if(clock_Update_Q == NULL) clock_Update_Q = xQueueCreate(10, sizeof(Clock_Colors));

  //ESP_LOGI(TAG, "ntp time zone is: %s \n", ntp_TimeZone);

  mtb_Read_Nvs_Struct("ntp TimeZone", ntp_TimeZone, sizeof(ntp_TimeZone));
  setenv("TZ", ntp_TimeZone, 1);
  tzset();

  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();
  sntp_set_time_sync_notification_cb(on_got_time);

  mtb_End_This_Service(thisService);
}
