#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "Arduino.h"
#include "mtb_ota.h"
#include "mtb_graphics.h"
#include "mtb_littleFs.h"
#include "mtb_engine.h"
#include "mtb_gif_parser.h"
#include "mtb_nvs.h"
#include "mtb_buzzer.h"

static const char TAG[] = "MTB-GHOTA";

    /* initialize our ghota config */
    ghota_config_t ghconfig = {
        "PIXLPAL-M1.bin",
        "spiffs.bin",
        "spiffs",
        "api.github.com",
        "Meterbit",
        "MTB-F1",
        1
    };

EXT_RAM_BSS_ATTR TaskHandle_t ota_Updating = NULL;
EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *otaUpdateApplication_App = new Mtb_Applications_FullScreen(ota_Update_Task, &ota_Updating, "GHOTA Update");

Mtb_FixedText_t* otaUpdateTextTop = new Mtb_FixedText_t(8, 39, Terminal8x12, GREEN);  //FREE THESE VARIABLES WHEN DONE WITH THE OTA APPLICATION
Mtb_FixedText_t* otaUpdateTextBot = new Mtb_FixedText_t(8, 52, Terminal8x12, GREEN);
Mtb_FixedText_t* otaUpdateTextBar = new Mtb_FixedText_t(105, 52, Terminal8x12, WHITE);

SemaphoreHandle_t ota_Update_Sem = NULL; 

String semver_t_ToString(const semver_t &version);

    static void ghota_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
        char softProgress[6] = {0};
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        ghota_client_handle_t *client = (ghota_client_handle_t *)handler_args;
        ESP_LOGI(TAG, "Got Update Callback: %s", ghota_get_event_str((ghota_event_e)id));

        if (id == GHOTA_EVENT_START_CHECK){
        statusBarNotif.mtb_Scroll_This_Text("CHECKING FOR SOFTWARE UPDATE", CYAN);
        }
        else if (id == GHOTA_EVENT_UPDATE_AVAILABLE){
            String updateAvailable = "{\"pxp_command\": 2, \"response\": 1, \"latVersion\": \"";
            /* get the version of the latest release on Github */
            semver_t *latestVer = ghota_get_latest_version(client);
            updateAvailable += semver_t_ToString(*latestVer) + "\"}";
            //latest_softwareVersion(dLatestVersion.c_str());
            //ESP_LOGI(TAG, "Latest Software Version: %s\n", dLatestVersion.c_str());
            if (latestVer) semver_free(latestVer);
            bleSettingsComSend(mtb_Software_Update_Route, updateAvailable); // Send the update available message to the BLE client
        }
        else if (id == GHOTA_EVENT_NOUPDATE_AVAILABLE){
            //String updateNotAvailable = "{\"pxp_command\": 2, \"response\": 1, \"available\": 0}";
            //statusBarNotif.mtb_Scroll_This_Text("PIXLPAL IS UP-TO-DATE", LEMON);
            if(Mtb_Applications::currentRunningApp == Mtb_Applications::otaAppHolder) mtb_Launch_This_App(Mtb_Applications::previousRunningApp, IGNORE_PREVIOUS_APP);
            xSemaphoreGiveFromISR(ota_Update_Sem, &xHigherPriorityTaskWoken);
            otaUpdateApplication_App->app_is_Running = pdFALSE;
            // bleSettingsComSend(mtb_Software_Update_Route, updateNotAvailable);
            // ESP_LOGI(TAG, "No new Update available from Ghota.\n");
        }
        else if (id == GHOTA_EVENT_START_UPDATE){
            do_beep(CLICK_BEEP);
            Mtb_Applications::appDestroy(Mtb_Applications::currentRunningApp);
            mtb_App_Init(Mtb_Applications::otaAppHolder);
            if(litFS_Ready) mtb_Draw_Local_Png({"/batIcons/otaStatus.png", 0, 0});
            otaUpdateTextTop->mtb_Write_String("SOFTWARE UPDATE");
            otaUpdateTextBot->mtb_Write_String("IN PROGRESS:");
            otaUpdateTextBar->mtb_Write_String("0%");
        }
        else if (id == GHOTA_EVENT_FIRMWARE_UPDATE_PROGRESS){
            //PNG_LocalImage_t updtBar_Icon {"/batIcons/updtBar_", 1, 53};
            sprintf(softProgress, "%d", *((int *)event_data));
            //sprintf(updtBar_Icon.imagePath + 27, "%d", *((int *)event_data));
            //strcat(updtBar_Icon.imagePath, ".png");
            strcat(softProgress, "%");
            //mtb_Draw_Local_Png(updtBar_Icon);  // The Progress Bars don't show because the OTA App is actively writing to the SoC's Flash. Reading from flash at same time is not possible.
            //ESP_LOGI(TAG, "Image Path is: %s\n", updtBar_Icon.imagePath);
            if(*((int *)event_data) < 100 ) otaUpdateTextBar->mtb_Write_String(softProgress); // We compare the percentage completion to 100 to prevent writing outside the pixel panel
            else otaUpdateTextBar->mtb_Write_String("OK");
        }
        else if (id == GHOTA_EVENT_FINISH_UPDATE){   
            // PNG_LocalImage_t wipeUpdtBar_Icon {"/littlefs/batIcons/wipeUpdtBar.png", 0, 53};
            // mtb_Draw_Local_Png(wipeUpdtBar_Icon);
            // ESP_LOGI(TAG, "Firmware Update Completed Successfully\n");   
        }
        else if (id == GHOTA_EVENT_UPDATE_FAILED){   
            //esp_restart();
            xSemaphoreGiveFromISR(ota_Update_Sem,&xHigherPriorityTaskWoken);
            mtb_Launch_This_App(Mtb_Applications::previousRunningApp, IGNORE_PREVIOUS_APP);
            statusBarNotif.mtb_Scroll_This_Text("SOFTWARE UPDATE FAILED DUE TO POOR NETWORK.    PIXLPAL WILL TRY AGAIN LATER.", RED);
            ESP_LOGI(TAG, "Firmware Update Failed\n");
            otaUpdateApplication_App->app_is_Running = pdFALSE;
        }
        // else if (id == GHOTA_EVENT_START_STORAGE_UPDATE){
        //     mtb_LittleFS_DeInit();
        //     otaUpdateTextTop->mtb_Write_Colored_String("STORAGE UPDATE", MAGENTA);
        //     otaUpdateTextBot->mtb_Write_Colored_String("IN PROGRESS:", MAGENTA);
        //     otaUpdateTextBar->mtb_Write_String("0%");
        //     ESP_LOGI(TAG, "Storage Update Started\n");
        // }
        // else if (id == GHOTA_EVENT_STORAGE_UPDATE_PROGRESS){
        //     sprintf(softProgress, "%d", *((int*)event_data));
        //     strcat(softProgress, "%");
        //     if(*((int *)event_data) < 100 ) otaUpdateTextBar->mtb_Write_String(softProgress);
        //     else otaUpdateTextBar->mtb_Write_String("OK");
        //     ESP_LOGI(TAG, "Storage Update In Progress\n");
        // }
        // else if(id == GHOTA_EVENT_FINISH_STORAGE_UPDATE){
        //     litFS_Ready = pdTRUE;
        //     mtb_Write_Nvs_Struct("litFS_Ready", &litFS_Ready, sizeof(uint8_t));
        //     mtb_LittleFS_Init();
        //     ESP_LOGI(TAG, "Storage Update Finished\n");
        // }
        // else if (id == GHOTA_EVENT_STORAGE_UPDATE_FAILED){
        //     litFS_Ready = pdFALSE;
        //     mtb_Write_Nvs_Struct("litFS_Ready", &litFS_Ready, sizeof(uint8_t));
        //     esp_restart();
        //     ESP_LOGI(TAG, "Storage Update Failed\n");
        // }
        else if (id == GHOTA_EVENT_PENDING_REBOOT){
            otaUpdateTextTop->mtb_Write_Colored_String("DEVICE UPDATED", CYAN);
            PNG_LocalImage_t wipeUpdtBar_Icon {"/batIcons/wipeUpdtBar.png", 0, 53};
            mtb_Draw_Local_Png(wipeUpdtBar_Icon);
            otaUpdateTextBot->mtb_Write_Colored_String("SUCCESSFULLY", CYAN);
            ESP_LOGI(TAG, "Ghota Pending Reboot.\n");
        }
        (void)client;
        return;
}

void ota_Update_Task(void* dApplication){
    Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
    if(ota_Update_Sem == NULL) ota_Update_Sem = xSemaphoreCreateBinary();
    Mtb_Applications::otaAppHolder = thisApp;
    Mtb_Applications::otaAppHolder->action_On_Prev_App = SUSPEND_PREVIOUS_APP;

    /* initialize ghota. */
    ghota_client_handle_t *ghota_client = ghota_init(&ghconfig);
    if (ghota_client == NULL) {
        ESP_LOGE(TAG, "ghota_client_init failed");
        return;
    }

    /* register for events relating to the update progress */
    esp_event_handler_register(GHOTA_EVENTS, ESP_EVENT_ANY_ID, &ghota_event_callback, ghota_client);

    /* for private repositories or to get more API calls than anonymouse, set a github username and PAT token
     * see https://docs.github.com/en/github/authenticating-to-github/creating-a-personal-access-token
     * for more information on how to create a PAT token.
     * 
     * Be carefull, as the PAT token will be stored in your firmware etc and can be used to access your github account.
     */
    ESP_ERROR_CHECK(ghota_set_auth(ghota_client, "Meterbit", github_Token));
    /* or do a check/update now
    * This runs in a new task under freeRTOS, so you can do other things while it is running.    */
    ESP_ERROR_CHECK(ghota_start_update_task(ghota_client));

// /* start a timer that will automatically check for updates based on the interval specified above */
//     if(litFS_Ready == pdFALSE) ESP_ERROR_CHECK(ghota_start_update_timer(ghota_client));   // Try doing a check every one minute to see how this function performs.
// /* check for updates immediately */
//     else ESP_ERROR_CHECK(ghota_start_update_task(ghota_client));

    xSemaphoreTake(ota_Update_Sem, portMAX_DELAY);

    ghota_free(ghota_client);

    while(MTB_APP_IS_ACTIVE == pdTRUE) delay(10);

    mtb_End_This_App(thisApp);// We are using this command, but this is an App not a service.
}


String semver_t_ToString(const semver_t &version) {
  String result = "v";  // Prefix with 'v'
  result += String(version.major) + "." +
            String(version.minor) + "." +
            String(version.patch);

  if (version.prerelease != nullptr && version.prerelease[0] != '\0') {
    result += "-";
    result += version.prerelease;
  }

  if (version.metadata != nullptr && version.metadata[0] != '\0') {
    result += "+";
    result += version.metadata;
  }

  return result;
}