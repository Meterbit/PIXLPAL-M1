#include "Arduino.h"
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "encoder.h"
#include "mtb_nvs.h"
#include "mtb_system.h"
#include "mtb_wifi.h"
#include "ArduinoJson.h"
#include "mtb_audio.h"
#include "mtb_buzzer.h"
#include "mtb_engine.h"
#include <ctime>
#include <string>
#include "NimBLEDevice.h"
#include "mtb_usb_ota.h"
//#include "esp_heap_caps.h"

static const char TAG[] = "METERBIT_ENGINE";

EXT_RAM_BSS_ATTR Mtb_User_AppID_t lastSavedApp = {0,1}; // Default to Calendar Clock App.
EXT_RAM_BSS_ATTR Mtb_Apps_To_Cycle_t cycling_Apps;

EXT_RAM_BSS_ATTR QueueHandle_t clock_Update_Q = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t appCycling_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t appLauncher_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t nvsAccess_Task_Handle = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t appLauncherQueue = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t nvsAccessQueue = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t nvsAccessComplete_Sem = NULL;

void (*mtb_Common_EncoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing;
void (*mtb_Common_ButtonFn_Ptr)(button_event_t) = buttonDoNothing;

EXT_RAM_BSS_ATTR Mtb_Services *mtb_App_Cycling_Sv = new Mtb_Services(appCyclingTask, &appCycling_Task_H, "App Cycling Task", 3072, 3);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_App_Launcher_Sv = new Mtb_Services(appLauncherTask, &appLauncher_Task_H, "App Launcher Task", 2560, 3);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Read_Write_NVS_Sv = new Mtb_Services(nvsAccessTask, &nvsAccess_Task_Handle, "NVS Access Tsk", 2560, 3);

EXT_RAM_BSS_ATTR Mtb_Applications* Mtb_Applications::otaAppHolder = nullptr;
EXT_RAM_BSS_ATTR Mtb_Applications* Mtb_Applications::currentRunningApp = nullptr;
EXT_RAM_BSS_ATTR Mtb_Applications* Mtb_Applications::previousRunningApp = nullptr;

// Device Status Flags
bool Mtb_Applications::internetConnectStatus = false;
bool Mtb_Applications::usbPenDriveConnectStatus = false;
bool Mtb_Applications::usbPenDriveMounted = false;
bool Mtb_Applications::pxpWifiConnectStatus = false;
bool Mtb_Applications::bleAdvertisingStatus = false;
bool Mtb_Applications::bleCentralContd = false;
uint16_t Mtb_Applications::bleCentralNegotiatedMtu = 512;  // Default to 512, can be updated based on actual negotiation results
uint8_t Mtb_Applications::firmwareOTA_Status = 6;
uint8_t Mtb_Applications::spiffsOTA_Status = 6;
Mtb_Action_On_App_t Mtb_Applications::actionOnApp = LAUNCH_PXP_APP;

Mtb_Applications::Mtb_Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, Mtb_User_AppID_t applicationID, uint32_t dStackSize, Mtb_Status_Bar_t statusBarType){
    application = dApplication;
    appHandle_ptr = dAppHandle_ptr;
    strcpy(appName, dAppName);
    appID = applicationID;
    stackSize = dStackSize;
    appPriority = 1;
    if(dAppHandle_ptr == (&usbFirmwareUpdate_H)) appCore = 1;
    else appCore = 0;
    appStatusBarType = statusBarType;
    elementRefresh = true;
}

Mtb_Applications* mtb_Launch_This_App(Mtb_Applications *dApp, Mtb_Action_On_App_t action, Mtb_Do_Prev_App_t do_Prv_App){
        Mtb_Applications::actionOnApp = action;
        if (*(dApp->appHandle_ptr) != NULL && eTaskGetState(*(dApp->appHandle_ptr)) == eSuspended){
        Mtb_Applications::appSuspend(Mtb_Applications::currentRunningApp);
        Mtb_Applications::appResume(dApp);
        } else {
        if(cycling_Apps.appsCycleActive == true && do_Prv_App != IGNORE_PREVIOUS_APP) dApp->action_On_Prev_App = SUSPEND_PREVIOUS_APP;
        else dApp->action_On_Prev_App = do_Prv_App;
        xQueueSend(appLauncherQueue, &dApp, portMAX_DELAY);
        mtb_Launch_This_Service(mtb_App_Launcher_Sv);
        }
        return dApp;
}

void mtb_Launch_This_Service(Mtb_Services* THIS_SERV){
    //ESP_LOGI(TAG, "Attempting to start service: %s\n", THIS_SERV->serviceName);
    BaseType_t result;
    if(*(THIS_SERV->serviceT_Handle_ptr) == NULL) {  // Prevents the service from being started multiple times
        MTB_SERV_IS_ACTIVE = pdTRUE;
        result = xTaskCreatePinnedToCore(THIS_SERV->service, THIS_SERV->serviceName, THIS_SERV->stackSize, THIS_SERV, THIS_SERV->servicePriority, THIS_SERV->serviceT_Handle_ptr, THIS_SERV->serviceCore);
        //if(result == pdPASS) ESP_LOGI(TAG, "Task %s successfully launched\n", THIS_SERV->serviceName);
        if(result != pdPASS) ESP_LOGE(TAG, "Task %s failed to launch with error code: %d\n", THIS_SERV->serviceName, result);
    }
}

void mtb_Resume_This_Service(Mtb_Services* THIS_SERV){
    vTaskResume(*(THIS_SERV->serviceT_Handle_ptr));
}

void mtb_Suspend_This_Service(Mtb_Services* THIS_SERV){
    vTaskSuspend(*(THIS_SERV->serviceT_Handle_ptr));
}

void appLauncherTask(void * dService){
    Mtb_Services *THIS_SERV = (Mtb_Services *)dService;
    Mtb_Applications *appLaunchHolder = nullptr;
    while (xQueueReceive(appLauncherQueue, &appLaunchHolder, pdMS_TO_TICKS(500))){
    if (Mtb_Applications::currentRunningApp != nullptr && Mtb_Applications::actionOnApp == LAUNCH_PXP_APP){
        Mtb_Applications::previousRunningApp = Mtb_Applications::currentRunningApp;
        Mtb_Applications::actionOnPreviousApp(appLaunchHolder->action_On_Prev_App);
    }
        appLaunchHolder->appRunner();
    }
    mtb_Delete_This_Service(THIS_SERV);
}

void appCyclingTask(void* dService){
    Mtb_Services *THIS_SERV = (Mtb_Services *)dService;
    Mtb_Applications *appCyclingHolder = nullptr;
    uint16_t appDurationCounter = 0;
    mtb_Show_Status_Bar_Icon({"/batIcons/appCycle.png", 27, 1});
    mtb_Identify_App_By_ID(Mtb_Applications::currentRunningApp->appID, KILL_PXP_APP);

        uint32_t p = 0;
        for(uint8_t i = 0; MTB_SERV_IS_ACTIVE == pdTRUE; ++p, i = p % cycling_Apps.appsNoInCycle){
            if(cycling_Apps.appsNoInCycle == 0) break;
            if (cycling_Apps.appsCycling[i].GenApp != 255 && cycling_Apps.appsCycling[i].SpeApp != 255){
                appDurationCounter = cycling_Apps.appCycleDuration * 100;
                appCyclingHolder = mtb_Identify_App_By_ID(cycling_Apps.appsCycling[i], LAUNCH_PXP_APP);
                if(p >= cycling_Apps.appsNoInCycle) mtb_Panel_Draw_FullScreen_RGB565(cycling_Apps.app_Frame_Buffers[i]);
                //printf("@@@@@@@@ Showing \"%s\" app in slot %d: GenApp %d, SpeApp %d\n", appCyclingHolder->appName, i, cycling_Apps.appsCycling[i].GenApp, cycling_Apps.appsCycling[i].SpeApp);
                while(appDurationCounter --> 0 && MTB_SERV_IS_ACTIVE == pdTRUE) delay(10);
                memcpy(cycling_Apps.app_Frame_Buffers[i], mtb_Panel_Frame_Buffer, sizeof(mtb_Panel_Frame_Buffer));
            } else continue;
        }

        for(uint8_t i = 0; i < cycling_Apps.appsNoInCycle; i++){
            if (cycling_Apps.appsCycling[i].GenApp != 255 && cycling_Apps.appsCycling[i].SpeApp != 255){
                appCyclingHolder = mtb_Identify_App_By_ID(cycling_Apps.appsCycling[i], KILL_PXP_APP);
                //printf("^^^^^^^ Killing app %s in slot %d: GenApp %d, SpeApp %d\n", appCyclingHolder->appName, i, cycling_Apps.appsCycling[i].GenApp, cycling_Apps.appsCycling[i].SpeApp);
            } else continue;
        }

    if(cycling_Apps.appsCycleActive == false){    
    mtb_Identify_App_By_ID(Mtb_Applications::currentRunningApp->appID, LAUNCH_PXP_APP);
    mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", 27, 1});
}


    mtb_Delete_This_Service(THIS_SERV);
}


void nvsAccessTask(void* dService){
    Mtb_Services *THIS_SERV = (Mtb_Services *)dService;
    NvsAccessParams_t nvsAccessObjHolder;
    while (xQueueReceive(nvsAccessQueue, &nvsAccessObjHolder, pdMS_TO_TICKS(500))){     // the pdMS_TO_TICKS(500) here waits for another nvs read/write command to be added before killing the task
        if(nvsAccessObjHolder.read_OR_Write == NVS_MEM_READ){
            nvs_handle read_nvs_handle;
            esp_err_t err = nvs_open("hx_list", NVS_READWRITE, &read_nvs_handle);
            size_t required_size = nvsAccessObjHolder.struct_size;
            esp_err_t error1 = nvs_get_blob(read_nvs_handle, nvsAccessObjHolder.key, nvsAccessObjHolder.struct_ptr, &required_size);
            nvs_close(read_nvs_handle);
            if(error1 == ESP_ERR_NVS_NOT_FOUND){
                nvs_handle write_nvs_handle;
                esp_err_t err = nvs_open("hx_list", NVS_READWRITE, &write_nvs_handle);
                error1 = nvs_set_blob(write_nvs_handle, nvsAccessObjHolder.key, nvsAccessObjHolder.struct_ptr, nvsAccessObjHolder.struct_size);
                err = nvs_commit(write_nvs_handle);
                nvs_close(write_nvs_handle);
            }
            if(error1 != ESP_OK){
                ESP_LOGI(TAG, "Error reading NVS: %s\n", esp_err_to_name(error1));
            }
            xSemaphoreGive(nvsAccessComplete_Sem);
        } else if(nvsAccessObjHolder.read_OR_Write == NVS_MEM_WRITE){
                nvs_handle write_nvs_handle;
                esp_err_t err = nvs_open("hx_list", NVS_READWRITE, &write_nvs_handle);
                esp_err_t error1 = nvs_set_blob(write_nvs_handle, nvsAccessObjHolder.key, nvsAccessObjHolder.struct_ptr, nvsAccessObjHolder.struct_size);
                err = nvs_commit(write_nvs_handle);
                nvs_close(write_nvs_handle);
                if(error1 != ESP_OK){
                ESP_LOGI(TAG, "Error writing NVS: %s\n", esp_err_to_name(error1));
            }
                xSemaphoreGive(nvsAccessComplete_Sem);
        }
    }
        mtb_Delete_This_Service(THIS_SERV);
}

bool Mtb_Applications::appRunner(){
    // Dynamic task creation
    if (xTaskCreatePinnedToCore(application, appName, stackSize, this, appPriority, appHandle_ptr, appCore) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create application task in INTERNAL DRAM: %s\n", appName);
        return false; 
    }
    //ESP_LOGI(TAG, "App \"%s\" Launched on core %d\n", appName, xPortGetCoreID());
    return true; 
}

void Mtb_Applications::appResume(Mtb_Applications* dApp){
    //ESP_LOGI(TAG, "App Resume on \"%s\" Called.\n", dApp->appName);

    previousRunningApp = currentRunningApp;

    if(dApp->appStatusBarType == CLOCK_STATUS_BAR){
        mtb_Draw_Status_Bar();
        mtb_Launch_This_Service(mtb_Status_Bar_Clock_Sv);
    } else if(dApp->appStatusBarType == WEEKDAY_STATUS_BAR){
        mtb_Draw_Status_Bar();
        mtb_Launch_This_Service(mtb_Status_Bar_Calendar_Sv);
    } else if(dApp->appStatusBarType == ICONS_ONLY_STATUS_BAR){
        mtb_Draw_Status_Bar();
    }

    if(litFS_Ready){
    vTaskResume(*(dApp->appHandle_ptr));
    for (Mtb_Services* THIS_SERV : dApp->appServices) if (THIS_SERV != nullptr) mtb_Resume_This_Service(THIS_SERV);
    }

    mtb_Common_EncoderFn_ptr = dApp->mtb_App_EncoderFn_ptr; 
    mtb_Common_ButtonFn_Ptr = dApp->mtb_App_ButtonFn_Ptr;

    // dApp->mtb_App_Set_Ble_Comm_Sv_Fns(clock_Color_Change, get_NTP_Local_Time);
    
    currentRunningApp = dApp;
    dApp->elementRefresh = true;
}

void Mtb_Applications::appSuspend(Mtb_Applications* dApp){
    //ESP_LOGI(TAG, "App Suspend on \"%s\" Called.\n", dApp->appName);

    if(*(dApp->appHandle_ptr) == NULL) return;

    for (Mtb_Services* servElement : dApp->appServices) if (servElement != nullptr) mtb_Suspend_This_Service(servElement);
    vTaskSuspend(*(dApp->appHandle_ptr));

    if(dApp->appStatusBarType == CLOCK_STATUS_BAR) mtb_Status_Bar_Clock_Sv->service_is_Running = pdFALSE;
    if(dApp->appStatusBarType == WEEKDAY_STATUS_BAR) mtb_Status_Bar_Calendar_Sv->service_is_Running = pdFALSE;
}

void Mtb_Applications::appDestroy(Mtb_Applications* dApp){

    if(*(dApp->appHandle_ptr) != NULL && dApp->app_is_Running == pdTRUE){
        dApp->app_is_Running = pdFALSE;
        while(*(dApp->appHandle_ptr) != NULL) delay(1);
    }

    for (Mtb_Services *servElement : dApp->appServices){
    if(servElement != nullptr && servElement->service_is_Running == pdTRUE){
        servElement->service_is_Running = pdFALSE;
        while(*(servElement->serviceT_Handle_ptr) != NULL) delay(1);
        }
    }

    if(dApp->appStatusBarType == CLOCK_STATUS_BAR) mtb_Status_Bar_Clock_Sv->service_is_Running = pdFALSE;
    else if(dApp->appStatusBarType == WEEKDAY_STATUS_BAR) mtb_Status_Bar_Calendar_Sv->service_is_Running = pdFALSE;

    for (uint8_t i = 0; i < 5; i++){
    if((scrollText_Handles[i]) != NULL){
        Mtb_ScrollText_t holder;           
        while (xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(1)) == pdTRUE){
                Mtb_ScrollText_t::scrollTask_HolderPointers[i]->scroll_Quit = pdTRUE;
                delay(1);
            } 
        }
    }

    //ESP_LOGW(TAG, "App Destroy on \"%s\" Called.\n", dApp->appName);
}

void Mtb_Applications::mtb_App_Show_Mobile_UI(void){
    char applicationID[20];
    String mobileUICommand;
    snprintf(applicationID, sizeof(applicationID), "%u/%u", appID.GenApp, appID.SpeApp);

    if(appMobile_Data == nullptr) mobileUICommand = "{\"app_command\": 252, \"manifest\": " + String(appMobile_Manifest) + ", \"ui_api\": " + String(appMobile_UiApi) + "}";
    else mobileUICommand = "{\"app_command\": 252, \"manifest\": " + String(appMobile_Manifest) + ", \"ui_api\": " + String(appMobile_UiApi) + ", \"data\": " + String(appMobile_Data) + "}";

    // ESP_LOGI(TAG, "Mobile UI Command: %s\n", mobileUICommand.c_str());
    
    bleApplicationComSend(applicationID, mobileUICommand);
}

void Mtb_Applications::actionOnPreviousApp(Mtb_Do_Prev_App_t dAction){
    if(litFS_Ready){
        switch (dAction){
        case SUSPEND_PREVIOUS_APP: Mtb_Applications::appSuspend(currentRunningApp);
            break;
        case DESTROY_PREVIOUS_APP: Mtb_Applications::appDestroy(currentRunningApp);
            break;
        case IGNORE_PREVIOUS_APP:
            ESP_LOGI(TAG, "Ignoring action on previous App.\n");
            break;
        default:
            ESP_LOGI(TAG, "No action on previous App specified.\n");
            break;
        }
    }
}

void Mtb_Applications::mtb_App_Set_EC11_Cb_Fns(mtb_Common_ButtonFn_Ptr_t but_Fn, encoderFn_ptr_t enc_Fn){
    mtb_App_EncoderFn_ptr = enc_Fn;
    mtb_App_ButtonFn_Ptr = but_Fn;
}

void Mtb_Applications::mtb_App_Set_Mobile_UI(const char* manifest, const char* ui_api, const char* data){
    appMobile_Manifest = manifest;
    appMobile_UiApi = ui_api;
    appMobile_Data = data;
}

// This function deletes an App task and frees its resources. IMPORTANT:Only call from within a App task.
void mtb_Delete_This_App(Mtb_Applications* dApp){
    // UBaseType_t hwm = uxTaskGetStackHighWaterMark(*(dApp->appHandle_ptr));
    // ESP_LOGW(TAG, "The %s App free stack min so far: %u bytes", dApp->appName, (unsigned)(hwm * sizeof(StackType_t)));
    dApp->app_is_Running = pdFALSE;
    *(dApp->appHandle_ptr) = NULL;
    //ESP_LOGW(TAG, "THIS APPLICATION HAS BEEN DELETED: %s \n", dApp->appName);
    vTaskDelete(NULL);
}

// This function deletes a service task and frees its resources. Call from outside the service task.
Mtb_Applications* mtb_Kill_This_App(Mtb_Applications* dApp){
    if (*(dApp->appHandle_ptr) != NULL && eTaskGetState(*(dApp->appHandle_ptr)) == eSuspended){
        Mtb_Applications::appResume(dApp);
        Mtb_Applications::appDestroy(dApp);
        } else if (*(dApp->appHandle_ptr) != NULL && eTaskGetState(*(dApp->appHandle_ptr)) != eSuspended){
        Mtb_Applications::appDestroy(dApp);
        } else ESP_LOGW(TAG, "The \"%s\" is not active, app handler is NULL.\n", dApp->appName);
    return dApp;
}

// This function deletes a service task and frees its resources. IMPORTANT:Only call from within a service task.
void mtb_Delete_This_Service(Mtb_Services* dService){
    // UBaseType_t hwm = uxTaskGetStackHighWaterMark(*(dService->serviceT_Handle_ptr));
    // ESP_LOGW(TAG, "The %s Service Task free stack min so far: %u bytes", dService->serviceName, (unsigned)(hwm * sizeof(StackType_t)));  
    dService->service_is_Running = pdFALSE;  
    *(dService->serviceT_Handle_ptr) = NULL;
    //ESP_LOGI(TAG, "THIS SERVICE HAS BEEN DELETED: %s \n", dService->serviceName);
    vTaskDelete(NULL);
}

// This function deletes a service task and frees its resources. Call from outside the service task.
void mtb_Kill_This_Service(Mtb_Services* dService){
    dService->service_is_Running = pdFALSE;
    //ESP_LOGI(TAG, "THIS SERVICE HAS BEEN KILLED: %s \n", dService->serviceName);
}

void encoderDoNothing(rotary_encoder_rotation_t){}
void buttonDoNothing(button_event_t){}

void mtb_Brightness_Control(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
        if(panelBrightness <= 250){ 
        panelBrightness += 5;
        mtb_Panel_Set_Brightness(panelBrightness); // 0-255
        mtb_Set_Status_RGB_LED(currentStatusLEDcolor);
        mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
        }
        if(panelBrightness >= 255) do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
        if(panelBrightness >= 7){
        panelBrightness -= 5;
        mtb_Panel_Set_Brightness(panelBrightness); //0-255
        mtb_Set_Status_RGB_LED(currentStatusLEDcolor);
        mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
        }
        if(panelBrightness <= 6) do_beep(CLICK_BEEP);
    }
}

void mtb_Vol_Control_Encoder(rotary_encoder_rotation_t direction){
if (direction == ROT_CLOCKWISE){
    if(deviceVolume <= 20){ 
    ++deviceVolume;
    if(audio != nullptr) audio->setVolume(deviceVolume);
    mtb_Write_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));
    } else if(deviceVolume >= 21) do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
    if(deviceVolume >= 1){
    --deviceVolume;
    if(audio != nullptr) audio->setVolume(deviceVolume);
    mtb_Write_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));
    } else if(deviceVolume <= 0) do_beep(CLICK_BEEP);
    }
    //ESP_LOGI(TAG, "Device Volume is: %d\n", deviceVolume);
}

void randomButtonControl(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            break;

            case BUTTON_PRESSED:
            break;

            case BUTTON_PRESSED_LONG:
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
//*************************************************************************************************************************************************************
void Mtb_Applications::mtb_App_Init(Mtb_Services* pointer_0, Mtb_Services* pointer_1,
    Mtb_Services* pointer_2, Mtb_Services* pointer_3, Mtb_Services* pointer_4,
    Mtb_Services* pointer_5, Mtb_Services* pointer_6, Mtb_Services* pointer_7, 
    Mtb_Services* pointer_8, Mtb_Services* pointer_9){

    if(actionOnApp == SHOW_PXP_APP_UI){
        mtb_App_Show_Mobile_UI();
        mtb_Delete_This_App(this);
        return;
    }

    currentRunningApp = this;

    mtb_Common_ButtonFn_Ptr = mtb_App_ButtonFn_Ptr;
    mtb_Common_EncoderFn_ptr = mtb_App_EncoderFn_ptr;

    appServices[0] = pointer_0;
    appServices[1] = pointer_1;
    appServices[2] = pointer_2;
    appServices[3] = pointer_3;
    appServices[4] = pointer_4;
    appServices[5] = pointer_5;
    appServices[6] = pointer_6;
    appServices[7] = pointer_7;
    appServices[8] = pointer_8;
    appServices[9] = pointer_9;

    delay(250);

    mtb_Panel_Clear_Screen();

    for (Mtb_Services *THIS_SERV : appServices) if (THIS_SERV != nullptr) mtb_Launch_This_Service(THIS_SERV);

    switch(appStatusBarType){
        case CLOCK_STATUS_BAR:
            mtb_Launch_This_Service(mtb_Status_Bar_Clock_Sv);  
            break;
        case WEEKDAY_STATUS_BAR:
            mtb_Launch_This_Service(mtb_Status_Bar_Calendar_Sv);
            break;
        default:
            break;
    }

    if(mtb_App_ButtonFn_Ptr != buttonDoNothing) mtb_Launch_This_Service(mtb_Button_Task_Sv);

    if(mtb_App_EncoderFn_ptr != encoderDoNothing) mtb_Launch_This_Service(mtb_Encoder_Task_Sv);

    if(fullScreen == false) mtb_Draw_Status_Bar();

    if(Mtb_Applications::currentRunningApp->appID.GenApp <= 11) mtb_Write_Nvs_Struct("mtbNvsUserApp", &Mtb_Applications::currentRunningApp->appID, sizeof(Mtb_User_AppID_t));

    app_is_Running = pdTRUE;

    //ESP_LOGI(TAG, "THIS APPLICATION HAS BEEN STARTED: %s \n", Mtb_Applications::currentRunningApp->appName);
}

//*************************************************************************************************************************************************************

void mtb_Ble_App_Cmd_Respond_Success(const char* appID, uint8_t commandNumber, uint8_t response ){
    String jsonString;
    JsonDocument doc;
    doc["app_command"] = commandNumber;
    doc["response"] = response;
    serializeJson(doc, jsonString);
    bleApplicationComSend(appID, jsonString);
}

void Mtb_Applications::mtb_App_Set_Ble_Comm_Sv_Fns(bleCom_Parser_Fns_Ptr Fn_0, bleCom_Parser_Fns_Ptr Fn_1, bleCom_Parser_Fns_Ptr Fn_2 , bleCom_Parser_Fns_Ptr Fn_3, bleCom_Parser_Fns_Ptr Fn_4, bleCom_Parser_Fns_Ptr Fn_5, bleCom_Parser_Fns_Ptr Fn_6, bleCom_Parser_Fns_Ptr Fn_7, bleCom_Parser_Fns_Ptr Fn_8, bleCom_Parser_Fns_Ptr Fn_9, bleCom_Parser_Fns_Ptr Fn_10, bleCom_Parser_Fns_Ptr Fn_11){
    bleAppComServiceFns[0] = Fn_0;
    bleAppComServiceFns[1] = Fn_1;
    bleAppComServiceFns[2] = Fn_2;
    bleAppComServiceFns[3] = Fn_3;
    bleAppComServiceFns[4] = Fn_4;
    bleAppComServiceFns[5] = Fn_5;
    bleAppComServiceFns[6] = Fn_6;
    bleAppComServiceFns[7] = Fn_7;
    bleAppComServiceFns[8] = Fn_8;
    bleAppComServiceFns[9] = Fn_9;
    bleAppComServiceFns[10] = Fn_10;
    bleAppComServiceFns[11] = Fn_11;
}

 uint8_t mtb_Add_App_To_Cycle_App_List( Mtb_User_AppID_t appToAdd){
    if(cycling_Apps.appsNoInCycle >= CYCLING_APP_SLOTS) return 0;
    cycling_Apps.appsCycling[cycling_Apps.appsNoInCycle] = appToAdd;
    cycling_Apps.appsNoInCycle++;
    return 1;
}

 uint8_t mtb_Remove_App_From_Cycle_App_List(Mtb_User_AppID_t appToRemove){
    for(uint8_t i = 0; i < cycling_Apps.appsNoInCycle; i++){
        if(cycling_Apps.appsCycling[i].GenApp == appToRemove.GenApp && cycling_Apps.appsCycling[i].SpeApp == appToRemove.SpeApp){
            bool wasActive = cycling_Apps.appsCycleActive;
            if(wasActive){
                mtb_Kill_This_Service(mtb_App_Cycling_Sv);
                uint16_t timeout = 500;  // 5 s max (500 × 10 ms)
                while(*(mtb_App_Cycling_Sv->serviceT_Handle_ptr) != NULL && timeout-- > 0) delay(10);
            }

            for(uint8_t j = i; j < cycling_Apps.appsNoInCycle - 1; j++){
                cycling_Apps.appsCycling[j] = cycling_Apps.appsCycling[j + 1];
                memcpy(cycling_Apps.app_Frame_Buffers[j], cycling_Apps.app_Frame_Buffers[j + 1], sizeof(cycling_Apps.app_Frame_Buffers[0]));
            }
            cycling_Apps.appsNoInCycle--;
            cycling_Apps.appsCycling[cycling_Apps.appsNoInCycle] = {255, 255};
            memset(cycling_Apps.app_Frame_Buffers[cycling_Apps.appsNoInCycle], 0, sizeof(cycling_Apps.app_Frame_Buffers[0]));

            if(wasActive && cycling_Apps.appsNoInCycle > 0) mtb_Launch_This_Service(mtb_App_Cycling_Sv);
            return 1;
        }
    }
    return 0;
}


Mtb_Applications* mtb_Identify_App_By_ID(Mtb_User_AppID_t dAppID, Mtb_Action_On_App_t action){
    Mtb_Applications::actionOnApp = action;
    switch(dAppID.GenApp){
        case 0: return mtb_Clk_Tim_AppLaunch(dAppID.SpeApp, action);
        case 1: return mtb_Msg_App_Launch(dAppID.SpeApp, action);
        case 2: return mtb_Calendar_App_Launch(dAppID.SpeApp, action);
        case 3: return mtb_Weather_App_Launch(dAppID.SpeApp, action);
        case 4: return mtb_Finance_App_Launch(dAppID.SpeApp, action);
        case 5: return mtb_Sports_App_Launch(dAppID.SpeApp, action);
        case 6: return mtb_Animations_App_Launch(dAppID.SpeApp, action);
        case 7: return mtb_Notifications_App_Launch(dAppID.SpeApp, action);
        case 8: return mtb_AIs_App_Launch(dAppID.SpeApp, action);
        case 9: return mtb_Audio_Stream_App_Launch(dAppID.SpeApp, action);
        case 10: return mtb_sMedia_App_Launch(dAppID.SpeApp, action);
        case 11: return mtb_Miscellanous_App_Launch(dAppID.SpeApp, action);
        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
        return nullptr;
}

//********NUMBER 0 */
Mtb_Applications* mtb_Clk_Tim_AppLaunch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){    
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(calendarClock_App, action);
        case 1: return mtb_Launch_This_App(pixelAnimClock_App, action);
        case 2: return mtb_Launch_This_App(worldClock_App, action);
        case 3: return mtb_Launch_This_App(bigClockCalendar_App, action);
        case 4: return mtb_Launch_This_App(stopWatch_App, action);
        case 5: return mtb_Launch_This_App(worldClock_App, action);
        default: ESP_LOGE(TAG, "No Apps to Launch.\n"); break;
        }
    } else if(action == KILL_PXP_APP) {
        switch(dAppNumber){
        case 0: return mtb_Kill_This_App(calendarClock_App);
        case 1: return mtb_Kill_This_App(pixelAnimClock_App);
        case 2: return mtb_Kill_This_App(worldClock_App);
        case 3: return mtb_Kill_This_App(bigClockCalendar_App);
        case 4: return mtb_Kill_This_App(stopWatch_App);
        case 5: return mtb_Kill_This_App(worldClock_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n"); break;
        }
    } else if(action == IDENTIFY_PXP_APP) {
        switch(dAppNumber){
        case 0: return calendarClock_App;
        case 1: return pixelAnimClock_App;
        case 2: return worldClock_App;
        case 3: return bigClockCalendar_App;
        case 4: return stopWatch_App;
        case 5: return worldClock_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n"); break;
        }
    }
        return nullptr;
}

//********NUMBER 1 */
Mtb_Applications* mtb_Msg_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(googleNews_App, action);
        case 1: return mtb_Launch_This_App(rssNewsApp, action);
        case 2: return mtb_Launch_This_App(newsAPI_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(googleNews_App);
        case 1: return mtb_Kill_This_App(rssNewsApp);
        case 2: return mtb_Kill_This_App(newsAPI_App);
        
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return googleNews_App;
        case 1: return rssNewsApp;
        case 2: return newsAPI_App;

        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 2 */
Mtb_Applications* mtb_Calendar_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(google_Calendar_App, action);
        case 1: return mtb_Launch_This_App(outlook_Calendar_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(google_Calendar_App);
        case 1: return mtb_Kill_This_App(outlook_Calendar_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return google_Calendar_App;
        case 1: return outlook_Calendar_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 3 */
Mtb_Applications* mtb_Weather_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(openWeather_App, action);
        case 1: return mtb_Launch_This_App(openMeteo_App, action);
        case 2: return mtb_Launch_This_App(googleWeather_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(openWeather_App);
        case 1: return mtb_Kill_This_App(openMeteo_App);
        case 2: return mtb_Kill_This_App(googleWeather_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return openWeather_App;
        case 1: return openMeteo_App;
        case 2: return googleWeather_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 4 */
Mtb_Applications* mtb_Finance_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(finnhub_Stats_App, action);
        case 1: return mtb_Launch_This_App(coinCap_Stats_App, action);
        case 2: return mtb_Launch_This_App(currencyExchange_App, action);
        case 3: return mtb_Launch_This_App(polygonFX_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(finnhub_Stats_App);
        case 1: return mtb_Kill_This_App(coinCap_Stats_App);
        case 2: return mtb_Kill_This_App(currencyExchange_App);
        case 3: return mtb_Kill_This_App(polygonFX_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return finnhub_Stats_App;
        case 1: return coinCap_Stats_App;
        case 2: return currencyExchange_App;
        case 3: return polygonFX_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 5 */
Mtb_Applications* mtb_Sports_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(liveFootbalScores_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(liveFootbalScores_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return liveFootbalScores_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
        return nullptr;
}

//********NUMBER 6 */
Mtb_Applications* mtb_Animations_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){    
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(studioLight_App, action);
        case 1: return mtb_Launch_This_App(worldFlags_App, action);
        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(studioLight_App);
        case 1: return mtb_Kill_This_App(worldFlags_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return studioLight_App;
        case 1: return worldFlags_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 7 */
Mtb_Applications* mtb_Notifications_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){   
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(apple_Notifications_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(apple_Notifications_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return apple_Notifications_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
        return nullptr;
}

//********NUMBER 8 */
Mtb_Applications* mtb_AIs_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
    if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){ 
        switch(dAppNumber){
        case 0: return mtb_Launch_This_App(chatGPT_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(chatGPT_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return chatGPT_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 9 */
Mtb_Applications* mtb_Audio_Stream_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
        if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){  
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(internetRadio_App, action);
        case 1: return mtb_Launch_This_App(musicPlayer_App, action);
        case 2: return mtb_Launch_This_App(audSpecAnalyzer_App, action);
        case 3: return mtb_Launch_This_App(spotify_App, action);
        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
        break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(internetRadio_App);
        case 1: return mtb_Kill_This_App(musicPlayer_App);
        case 2: return mtb_Kill_This_App(audSpecAnalyzer_App);
        case 3: return mtb_Kill_This_App(spotify_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return internetRadio_App;
        case 1: return musicPlayer_App;
        case 2: return audSpecAnalyzer_App;
        case 3: return spotify_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
    }
}
    return nullptr;
}

//********NUMBER 10 */
Mtb_Applications* mtb_sMedia_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
        if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){  
    switch(dAppNumber){
        // case 0: return mtb_Launch_This_App(calendarClock_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        // case 0: return mtb_Kill_This_App(calendarClock_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        // case 0: return mtb_Kill_This_App(calendarClock_App);
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

//********NUMBER 11 */
Mtb_Applications* mtb_Miscellanous_App_Launch(uint16_t dAppNumber, Mtb_Action_On_App_t action){
        if(action == LAUNCH_PXP_APP || action == SHOW_PXP_APP_UI){  
    switch(dAppNumber){
        case 0: return mtb_Launch_This_App(exampleDesignMobileUI_App, action);

        default: ESP_LOGE(TAG, "No Apps to Launch.\n");
            break;
    }
} else if(action == KILL_PXP_APP) {
    switch(dAppNumber){
        case 0: return mtb_Kill_This_App(exampleDesignMobileUI_App);
        default: ESP_LOGE(TAG, "No Apps to Kill.\n");
        break;
    }
} else if(action == IDENTIFY_PXP_APP) {
    switch(dAppNumber){
        case 0: return exampleDesignMobileUI_App;
        default: ESP_LOGE(TAG, "No Apps to Identify.\n");
        break;
    }
}
    return nullptr;
}

esp_err_t mtb_Write_Nvs_Struct(const char* keyIdentifier, void* struct_pointer, size_t struct_sized) {
    NvsAccessParams_t dataWriteHolder{
        .read_OR_Write = NVS_MEM_WRITE,
        .key = keyIdentifier,
        .struct_ptr = struct_pointer,
        .struct_size = struct_sized
    };
    xQueueSend(nvsAccessQueue, &dataWriteHolder, pdMS_TO_TICKS(5000));
    mtb_Launch_This_Service(mtb_Read_Write_NVS_Sv);
    xSemaphoreTake(nvsAccessComplete_Sem, portMAX_DELAY);
    return 0;
}

esp_err_t mtb_Read_Nvs_Struct(const char* keyIdentifier, void* struct_pointer, size_t struct_sized) {
    NvsAccessParams_t dataReadHolder{
        .read_OR_Write = NVS_MEM_READ,
        .key = keyIdentifier,
        .struct_ptr = struct_pointer,
        .struct_size = struct_sized
    };
    xQueueSend(nvsAccessQueue, &dataReadHolder, pdMS_TO_TICKS(5000));
    mtb_Launch_This_Service(mtb_Read_Write_NVS_Sv);
    xSemaphoreTake(nvsAccessComplete_Sem, portMAX_DELAY);
    return 0;
}

String mtb_Unix_Time_To_Readable(time_t unixTime, int timezoneOffsetHours) {
    struct tm *tm_info;
    char buffer[26];

    // Adjust the UNIX time by the timezone offset (in seconds)
    time_t adjustedTime = unixTime + timezoneOffsetHours * 3600;

    // Convert adjusted UNIX time to a tm structure (UTC)
    tm_info = gmtime(&adjustedTime);

    // Format time as "YYYY-MM-DD HH:MM:SS"
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // Convert char array to String and return
    return String(buffer);
}

// Function to format large numbers
String mtb_Format_Large_Number(double number) {
  const char* suffixes[] = {"", "k", "m", "b", "t", "q"}; // Add more suffixes if needed
  int suffixIndex = 0;

  // Reduce the number and find the appropriate suffix
  while (number >= 1000.0 && suffixIndex < 5) {
    number /= 1000.0;
    suffixIndex++;
  }

  // Round to two decimal places
  number = round(number * 100) / 100;

  // Convert to string with suffix
  return String(number, 2) + suffixes[suffixIndex];
}

String mtb_Format_Iso_Date(const String& input) {
  struct tm timeinfo = {0};
  time_t parsedTime;
  bool hasTime = input.indexOf('T') != -1;

  if (hasTime) {
    // Format: "2025-06-10T10:25:00-07:00"
    int year, month, day, hour, min, sec, tzHour, tzMin;
    char tzSign;

    if (sscanf(input.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
               &year, &month, &day, &hour, &min, &sec,
               &tzSign, &tzHour, &tzMin) != 9) {
      return "Invalid dateTime";
    }

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon  = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = min;
    timeinfo.tm_sec  = sec;

    int offset = (tzHour * 3600 + tzMin * 60) * (tzSign == '-' ? 1 : -1);
    parsedTime = mktime(&timeinfo) + offset;
  } else {
    // Format: "2025-06-10"
    int year, month, day;
    if (sscanf(input.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
      return "Invalid date";
    }

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon  = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min  = 0;
    timeinfo.tm_sec  = 0;

    parsedTime = mktime(&timeinfo);
  }

  // Adjust to Nigeria time zone (UTC+1)
  parsedTime += 3600;
  struct tm* local = gmtime(&parsedTime);

  char buffer[32];
  strftime(buffer, sizeof(buffer), "%a %d %b", local);  // e.g., "Tue 10 Jun"
  return String(buffer);
}


String mtb_Format_Iso_Time(const String& input) {
  struct tm timeinfo = {0};
  time_t parsedTime;

  bool hasTime = input.indexOf('T') != -1;

  if (hasTime) {
    // Example: "2025-06-08T23:30:00+01:00"
    int year, month, day, hour, min, sec, tzHour, tzMin;
    char tzSign;

    if (sscanf(input.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
               &year, &month, &day, &hour, &min, &sec,
               &tzSign, &tzHour, &tzMin) != 9) {
      return "Invalid time";
    }

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = min;
    timeinfo.tm_sec = sec;

    // Apply timezone offset from the input itself
    int offsetSeconds = (tzHour * 3600 + tzMin * 60) * (tzSign == '-' ? 1 : -1);
    parsedTime = mktime(&timeinfo) - offsetSeconds;
  } else {
    return "All day";
  }

  // No manual timezone correction needed here — input already handled it
  struct tm* local = gmtime(&parsedTime);

  char buffer[16];
  strftime(buffer, sizeof(buffer), "%I:%M %p", local);
  if (buffer[0] == '0') {
    memmove(buffer, buffer + 1, strlen(buffer));  // Remove leading zero
  }

  return String(buffer);
}

String mtb_Format_Date_From_Timestamp(time_t timestamp) {
  // Adjust to Nigeria time zone (UTC+1)
  timestamp += 3600;

  struct tm* local = localtime(&timestamp);
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%a %d %b", local);  // e.g., "Tue 10 Jun"

  return String(buffer);
}

String mtb_Format_Time_From_Timestamp(time_t timestamp) {
  struct tm* local = localtime(&timestamp);
  char buffer[6];  // "HH:MM" + null terminator
  strftime(buffer, sizeof(buffer), "%H:%M", local);  // 24-hour format
  return String(buffer);
}



String mtb_Url_Encode(const char* str) {
    String encoded = "";
    char c;
    while ((c = *str++)) {
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        encoded += c;
      } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
        encoded += buf;
      }
    }
    return encoded;
  }

String mtb_Get_Current_Time_RFC3339() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      ESP_LOGI(TAG, "Failed to obtain time from NTP\n");
      return "";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buffer);
  }