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
#include "mtb_graphics.h"
#include "mtb_buzzer.h"
#include "mtb_engine.h"
#include <ctime>
#include <string>
//#include "esp_heap_caps.h"

static const char TAG[] = "METERBIT_ENGINE";

Mtb_CurrentApp_t currentApp{
    .GenApp = 0,
    .SpeApp = 1
    };
    
EXT_RAM_BSS_ATTR QueueHandle_t clock_Update_Q = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t ble_AppCom_Parser_Task_Handle = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t appLuncher_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t nvsAccess_Task_Handle = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t freeServAndAppPSRAM_Handle = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t appLuncherQueue = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t nvsAccessQueue = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t freeServAndAppPSRAM_Q = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t nvsAccessComplete_Sem = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t running_App_BLECom_Queue = NULL;
//EXT_RAM_BSS_ATTR TimerHandle_t bleRestoreTimer_H = NULL;

void (*encoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing;
void (*buttonFn_ptr)(button_event_t) = buttonDoNothing;

EXT_RAM_BSS_ATTR Mtb_Services *app_Luncher_Task_Sv = new Mtb_Services(appLuncherTask, &appLuncher_Task_H, "App Luncher Task", 4096, 3);
EXT_RAM_BSS_ATTR Mtb_Services *read_Write_NVS_Sv = new Mtb_Services(nvsAccessTask, &nvsAccess_Task_Handle, "NVS Access Tsk", 4096, 3);
EXT_RAM_BSS_ATTR Mtb_Services *freeServAndAppPSRAM_Sv = new Mtb_Services(freeServAndAppPSRAM_Task, &freeServAndAppPSRAM_Handle, "Free AppServPSRAM", 4096, 3);

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
uint8_t Mtb_Applications::firmwareOTA_Status = 6;
uint8_t Mtb_Applications::spiffsOTA_Status = 6;

Mtb_Applications::Mtb_Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, uint32_t dStackSize, uint8_t psRamStack, uint8_t core){
    application = dApplication;
    appHandle_ptr = dAppHandle_ptr;
    strcpy(appName, dAppName);
    stackSize = dStackSize;
    appPriority = 1;
    appCore = core;
    elementRefresh = true;
    usePSRAM_Stack = psRamStack;
}

// void bleRestoreTimerCallBkFn(TimerHandle_t bleRstTim){
//     //Launch the Calendar Clock App to Restore BLE Link.
//     const char calendarClockAppBleCom[] = "0/0|{\"app_command\":255}";
//     mtb_BleCom_Data_Trans_t com_Data;

//     com_Data.pay_size = strlen(calendarClockAppBleCom);
//     com_Data.payload = heap_caps_calloc(com_Data.pay_size + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
//     memcpy(com_Data.payload, calendarClockAppBleCom, com_Data.pay_size);

//     xQueueSend(appCom_queue, &com_Data, portMAX_DELAY);
//     mtb_Start_This_Service(mtb_Ble_AppComm_Parser_Sv);
// }

void mtb_Launch_This_App(Mtb_Applications *dApp, Mtb_Do_Prev_App_t do_Prv_App){
    dApp->action_On_Prev_App = do_Prv_App;
    xQueueSend(appLuncherQueue, &dApp, portMAX_DELAY);
    mtb_Start_This_Service(app_Luncher_Task_Sv);
}

// void mtb_Start_This_Service(Mtb_Services* dService){
//     if(*(dService->serviceT_Handle_ptr) == NULL) {
//         dService->service_is_Running = pdTRUE;
//         if(dService->usePSRAM_Stack == pdFALSE) xTaskCreatePinnedToCore(dService->service, dService->serviceName, dService->stackSize, dService, dService->servicePriority, dService->serviceT_Handle_ptr, dService->serviceCore);
//         else {
//         dService->task_stack = (StackType_t *)heap_caps_malloc(dService->stackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//         dService->tcb_psram = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
//         if (dService->task_stack == NULL || dService->tcb_psram == NULL){ 
//             ESP_LOGI(TAG, "Failed to allocate task/tcb stack in PSRAM\n"); 
//             return;
//             }
//         // Create the task with the stack in PSRAM
//         *dService->serviceT_Handle_ptr = xTaskCreateStaticPinnedToCore(dService->service, dService->serviceName, dService->stackSize, dService, dService->servicePriority, dService->task_stack, dService->tcb_psram, dService->serviceCore);
//         }
//     }
// }

void mtb_Start_This_Service(Mtb_Services* dService){
    if(*(dService->serviceT_Handle_ptr) == NULL) {  // Prevents the service from being started multiple times
        dService->service_is_Running = pdTRUE;
        if(dService->usePSRAM_Stack == pdFALSE) {xTaskCreatePinnedToCore(dService->service, dService->serviceName, dService->stackSize, dService, dService->servicePriority, dService->serviceT_Handle_ptr, dService->serviceCore);
        } else {
            dService->task_stack = (StackType_t *)heap_caps_malloc(dService->stackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            dService->tcb_psram = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
            if (dService->task_stack == NULL || dService->tcb_psram == NULL){ 
                ESP_LOGI(TAG, "Failed to allocate task/tcb stack in PSRAM\n"); 
                return;
            }
            // Create the task with the stack in PSRAM
            *dService->serviceT_Handle_ptr = xTaskCreateStaticPinnedToCore(dService->service, dService->serviceName, dService->stackSize, dService, dService->servicePriority, dService->task_stack, dService->tcb_psram, dService->serviceCore);
        }
    }
}

void mtb_Resume_This_Service(Mtb_Services* dService){
    vTaskResume(*(dService->serviceT_Handle_ptr));
}

void mtb_Suspend_This_Service(Mtb_Services* dService){
    vTaskSuspend(*(dService->serviceT_Handle_ptr));
}

void freeServAndAppPSRAM_Task(void * dService){
    Mtb_Services *thisService = (Mtb_Services *)dService;
    void * dMemorySet = nullptr;
    while (xQueueReceive(freeServAndAppPSRAM_Q, &dMemorySet, pdMS_TO_TICKS(500))){
    delay(1000); // Wait for 1 seconds before freeing memory of services and Mtb_Applications whose TCB and Stack were allocated in PSRAM.
    heap_caps_free(dMemorySet);
    }
    mtb_End_This_Service(thisService);
}

void appLuncherTask(void * dService){
    Mtb_Services *thisService = (Mtb_Services *)dService;
    Mtb_Applications *appLunchHolder = nullptr;
    while (xQueueReceive(appLuncherQueue, &appLunchHolder, pdMS_TO_TICKS(500))){
    if (Mtb_Applications::currentRunningApp != nullptr){
        Mtb_Applications::previousRunningApp = Mtb_Applications::currentRunningApp;
        Mtb_Applications::actionOnPreviousApp(appLunchHolder->action_On_Prev_App);
    }
    //ESP_LOGI(TAG, "About to run the app: %s\n", appLunchHolder->appName);
    appLunchHolder->appRunner();       
    //ESP_LOGI(TAG, "Application %s has been launched\n", appLunchHolder->appName);
    }
    mtb_End_This_Service(thisService);
}

void nvsAccessTask(void * dService){
    Mtb_Services *thisService = (Mtb_Services *)dService;
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
        }
        else if(nvsAccessObjHolder.read_OR_Write == NVS_MEM_WRITE){
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
        mtb_End_This_Service(thisService);
}

bool Mtb_Applications::appRunner(){
    if (usePSRAM_Stack == pdFALSE) {
        // Dynamic task creation
        if (xTaskCreatePinnedToCore(application, appName, stackSize, this, appPriority, appHandle_ptr, appCore) != pdPASS) {
            return false; // Task creation failed
        }
        //ESP_LOGI(TAG, "DSRAM APP CREATED.\n");
    } else {
        // Allocate stack and TCB in PSRAM/internal memory
        task_stack = (StackType_t *)heap_caps_malloc(stackSize * sizeof(StackType_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        tcb_psram = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
        if (task_stack == NULL || tcb_psram == NULL) {
            // Clean up allocated memory if partial allocation occurs
            if (task_stack) free(task_stack);
            if (tcb_psram) free(tcb_psram);
            return false; // Allocation failed
        }
        
        // Create the task with the allocated stack and TCB
        *appHandle_ptr = xTaskCreateStaticPinnedToCore(application, appName, stackSize, this, appPriority, task_stack, tcb_psram, appCore);
        if (*appHandle_ptr == NULL) {
            // Task creation failed, free allocated memory
            free(task_stack);
            free(tcb_psram);
            return false;
        }
        //ESP_LOGI(TAG, "PSRAM APP CREATED.\n");
    }

    return true; // Task creation successful
}

void Mtb_Applications::appResume(Mtb_Applications* dApp){
    appDestroy(currentRunningApp);
    dma_display->clearScreen();
    previousRunningApp = currentRunningApp;

    if(dApp->fullScreen == false) mtb_Draw_Status_Bar();

    if(litFS_Ready){
    vTaskResume(*(dApp->appHandle_ptr));
    for (Mtb_Services* element : dApp->appServices) if (element != nullptr) mtb_Resume_This_Service(element);
    }

    encoderFn_ptr = dApp->mtb_App_EncoderFn_ptr; // When an app is destroyed, we disable the rotary encoder.
    buttonFn_ptr = dApp->mtb_App_ButtonFn_ptr; // When an app is destroyed, we disable the rotary encoder.
    
    currentRunningApp = dApp;
    dApp->elementRefresh = true;
}

void Mtb_Applications::appSuspend(Mtb_Applications* dApp){
    for (Mtb_Services* element : dApp->appServices) if (element != nullptr) mtb_Suspend_This_Service(element);
    vTaskSuspend(*(dApp->appHandle_ptr));
}


void Mtb_Applications::appDestroy(Mtb_Applications* dApp){

    if(*(dApp->appHandle_ptr) != NULL && dApp->app_is_Running == pdTRUE){
        dApp->app_is_Running = pdFALSE;
        while(*(dApp->appHandle_ptr) != NULL) delay(1);
    }

    ESP_LOGI(TAG, "APP DESTROY FIRST STAGE COMPLETED\n");

    for (Mtb_Services *element : dApp->appServices){
    if(element != nullptr && element->service_is_Running == pdTRUE){
        element->service_is_Running = pdFALSE;
        while(*(element->serviceT_Handle_ptr) != NULL) delay(1);
        }
    }
    
    ESP_LOGI(TAG, "APP DESTROY SECOND STAGE COMPLETED\n");

    // for (uint8_t i = 0; i < 5; i++){
    //     if (*(scroll_Tasks_Sv[i]->serviceT_Handle_ptr) != NULL){
    //         while (*(scroll_Tasks_Sv[i]->serviceT_Handle_ptr) != NULL){
    //             Mtb_ScrollText_t::scrollTask_HolderPointers[i]->scroll_Quit = pdTRUE;
    //             delay(1);
    //         }
    //     }
    // }

    for (uint8_t i = 0; i < 5; i++){
    if((scrollText_Handles[i]) != NULL){
        Mtb_ScrollText_t holder;           
        while (xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(1)) == pdTRUE){
                Mtb_ScrollText_t::scrollTask_HolderPointers[i]->scroll_Quit = pdTRUE;
                delay(1);
            } 
        }
    }

    ESP_LOGI(TAG, "APP DESTROY COMPLETED SUCCESSFULLY.\n");

}

void Mtb_Applications::actionOnPreviousApp(Mtb_Do_Prev_App_t dAction){
    if(litFS_Ready){
        switch (dAction){
        case SUSPEND_PREVIOUS_APP: Mtb_Applications::appSuspend(currentRunningApp);
            break;
        case DESTROY_PREVIOUS_APP: Mtb_Applications::appDestroy(currentRunningApp);
            //ESP_LOGI(TAG, "Action on previous App Called.\n");
            break;
        default:
            ESP_LOGI(TAG, "No action on previous App specified.\n");
            break;
        }
    }
}

void mtb_End_This_App(Mtb_Applications* dApp){
        *(dApp->appHandle_ptr) = NULL;    
        if(dApp->usePSRAM_Stack == pdTRUE){
            xQueueSend(freeServAndAppPSRAM_Q, &dApp->task_stack, pdMS_TO_TICKS(500));
            xQueueSend(freeServAndAppPSRAM_Q, &dApp->tcb_psram, pdMS_TO_TICKS(500));
            mtb_Start_This_Service(freeServAndAppPSRAM_Sv);
            dApp->task_stack = NULL;
            dApp->tcb_psram = NULL;
        }
        //ESP_LOGI(TAG, "@@@@@@@@@@@@@@ THIS APPLICATION HAS BEEN DELETED: %s \n", dApp->appName);
        vTaskDelete(NULL);
}

void mtb_End_This_Service(Mtb_Services* dService){
        *(dService->serviceT_Handle_ptr) = NULL;
        if (dService->usePSRAM_Stack == pdTRUE){
            xQueueSend(freeServAndAppPSRAM_Q, &dService->task_stack, pdMS_TO_TICKS(500));
            xQueueSend(freeServAndAppPSRAM_Q, &dService->tcb_psram, pdMS_TO_TICKS(500));
            mtb_Start_This_Service(freeServAndAppPSRAM_Sv);
            dService->task_stack = NULL;
            dService->tcb_psram = NULL;
        }
        //ESP_LOGI(TAG, "************* THIS SERVICE HAS BEEN DELETED: %s \n", dService->serviceName);
        vTaskDelete(NULL);
}

void encoderDoNothing(rotary_encoder_rotation_t){}
void buttonDoNothing(button_event_t){}

void mtb_Brightness_Control(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
        if(panelBrightness <= 250){ 
        panelBrightness += 5;
        dma_display->setBrightness(panelBrightness); // 0-255
        mtb_Set_Status_RGB_LED(currentStatusLEDcolor);
        mtb_Write_Nvs_Struct("pan_brghnss", &panelBrightness, sizeof(uint8_t));
        }
        if(panelBrightness >= 255) do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
        if(panelBrightness >= 7){
        panelBrightness -= 5;
        dma_display->setBrightness(panelBrightness); //0-255
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
    }
    if(deviceVolume >= 21) do_beep(CLICK_BEEP);
} else if(direction == ROT_COUNTERCLOCKWISE){
    if(deviceVolume >= 1){
    --deviceVolume;
    if(audio != nullptr) audio->setVolume(deviceVolume);
    mtb_Write_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));
    }
    if(deviceVolume <= 0) do_beep(CLICK_BEEP);
}
    //ESP_LOGI(TAG, "Device Volume is: %d\n", deviceVolume);
}

void randomButtonControl(button_event_t button_Data){
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
//*************************************************************************************************************************************************************
void mtb_App_Init(Mtb_Applications *thisApp, Mtb_Services* pointer_0, Mtb_Services* pointer_1,
    Mtb_Services* pointer_2, Mtb_Services* pointer_3, Mtb_Services* pointer_4,
    Mtb_Services* pointer_5, Mtb_Services* pointer_6, Mtb_Services* pointer_7, 
    Mtb_Services* pointer_8, Mtb_Services* pointer_9){

    Mtb_Applications::currentRunningApp = thisApp;

    buttonFn_ptr = thisApp->mtb_App_ButtonFn_ptr;
    encoderFn_ptr = thisApp->mtb_App_EncoderFn_ptr;

    thisApp->appServices[0] = pointer_0;
    thisApp->appServices[1] = pointer_1;
    thisApp->appServices[2] = pointer_2;
    thisApp->appServices[3] = pointer_3;
    thisApp->appServices[4] = pointer_4;
    thisApp->appServices[5] = pointer_5;
    thisApp->appServices[6] = pointer_6;
    thisApp->appServices[7] = pointer_7;
    thisApp->appServices[8] = pointer_8;
    thisApp->appServices[9] = pointer_9;

    delay(250);
    dma_display->clearScreen();
    for (Mtb_Services *element : thisApp->appServices) if (element != nullptr) mtb_Start_This_Service(element);
    if(thisApp->mtb_App_ButtonFn_ptr != buttonDoNothing) mtb_Start_This_Service(button_Task_Sv);
    if(thisApp->mtb_App_EncoderFn_ptr != encoderDoNothing) mtb_Start_This_Service(encoder_Task_Sv);
    if(thisApp->fullScreen == false) mtb_Draw_Status_Bar();
    MTB_APP_IS_ACTIVE = pdTRUE;
    //ESP_LOGI(TAG, "&&&&&&&&&&&&& THIS APPLICATION HAS BEEN STARTED: %s \n", Mtb_Applications::currentRunningApp->appName);
}

//*************************************************************************************************************************************************************

void mtb_Ble_App_Cmd_Respond_Success(const char* appRoute, uint8_t commandNumber, uint8_t response ){
    String jsonString;
    StaticJsonDocument<1024> doc;

    doc["pxp_command"] = commandNumber;
    doc["response"] = response;
    serializeJson(doc, jsonString);
    bleApplicationComSend(appRoute, jsonString);
}

void ble_AppCom_Parse_Task(void* dService){
    Mtb_Services *thisService = (Mtb_Services *)dService;
    mtb_BleCom_Data_Trans_t qMessage;
    DeserializationError dError;
    String dNewAppParams;
    uint16_t dAppGen = 0;
    uint16_t dAppSpe = 0;
    uint16_t dCmd_num = 0;

    while(xQueueReceive(appCom_queue, &qMessage, pdMS_TO_TICKS(500))){
        //ESP_LOGI(TAG, "Application Payload is:  %s\n", (char*) qMessage.payload);
        String dInstruction = String((char *)qMessage.payload);
        int charIndex = dInstruction.indexOf('|');             // find index of target character
        String specify_Application = dInstruction.substring(0, charIndex);  // copy up to the target character
        String dJsonPayload = dInstruction.substring(++charIndex);

        // ESP_LOGI(TAG, "The specific App is: %s\n", specify_Application.c_str());
        // ESP_LOGI(TAG, "The dPayload for App is: %s\n", dPayload.c_str());
 
        dAppGen = getIntegerAtIndex(specify_Application, 0);
        dAppSpe = getIntegerAtIndex(specify_Application, 1);

        //ESP_LOGI(TAG, "The dAppGen is: %d\n", dAppGen);
        //ESP_LOGI(TAG, "The dAppSpe is: %d\n", dAppSpe);

        if (dAppGen == currentApp.GenApp && dAppSpe == currentApp.SpeApp){

            dError = deserializeJson(dCommand, dJsonPayload);
            if(dError.code() == dError.Ok) dCmd_num = dCommand["app_command"];
            else dCmd_num = 0xFFFF;

            switch (dCmd_num){
            case 0: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[0] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[0](dCommand);
                break;
            case 1: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[1] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[1](dCommand);
                break;
            case 2: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[2] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[2](dCommand);
                break;
            case 3: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[3] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[3](dCommand);
                break;
            case 4: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[4] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[4](dCommand);
              break;
            case 5: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[5] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[5](dCommand);
                break;
            case 6: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[6] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[6](dCommand);
                break;
            case 7: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[7] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[7](dCommand);
                break;
            case 8: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[8] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[8](dCommand);
                break;
            case 9: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[9] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[9](dCommand);
                break;
            case 10: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[10] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[10](dCommand);
                break;
            case 11: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[11] != nullptr) mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[11](dCommand);
                break;
            // case 12: if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[12] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[12](dCommand);;     // These are for extras or future upgrades.
            //     break;
            // case 13:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[13] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[13](dCommand);
            //     break;
            // case 14:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[4] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[14](dCommand);
            //     break;
            // case 15:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[15] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[15](dCommand);
            //     break;
            // case 16:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[16] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[16](dCommand);
            //     break;
            // case 17:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[17] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[17](dCommand);
            //     break;
            // case 18:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[18] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[18](dCommand);
            //     break;
            // case 19:  if(mtb_Ble_AppComm_Parser_Sv->bleAppComServiceFns[19] != nullptr) mtb_Ble_AppComm_Parser_Sv.bleAppComServiceFns[19](dCommand);
            //     break;
            case 255:
                statusBarNotif.mtb_Scroll_This_Text("APP IS ALREADY ACTIVE", CYAN);
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 255}");
                break;
            default: statusBarNotif.mtb_Scroll_This_Text("ERROR: ASSESS COMMAND PARAMETERS", YELLOW);
                break;
            }
        }else{
            dError = deserializeJson(dCommand, dJsonPayload);
            dCmd_num = dCommand["app_command"];

            if (dError.code() == dError.Ok && dCmd_num == 0xFF){
                currentApp.GenApp = getIntegerAtIndex(specify_Application, 0);
                currentApp.SpeApp = getIntegerAtIndex(specify_Application, 1);
                mtb_Write_Nvs_Struct("currentApp", &currentApp, sizeof(Mtb_CurrentApp_t));
                mtb_General_App_Lunch(currentApp);
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 253}");
            }else{
                bleApplicationComSend(specify_Application.c_str(), "{\"pxp_command\": 254}");
                statusBarNotif.mtb_Scroll_This_Text("TAP 'LAUNCH' TO START APP", MAGENTA);
            } 
        }
    vTaskDelay(1);
    free(qMessage.payload);
    //qMessage.payload = NULL; // Set pointer to NULL to avoid dangling pointer    
  }
  mtb_End_This_Service(thisService);
}

void Mtb_Service_With_Fns::mtb_Register_Ble_Comm_ServiceFns(bleCom_Parser_Fns_Ptr Fn_0, bleCom_Parser_Fns_Ptr Fn_1, bleCom_Parser_Fns_Ptr Fn_2 , bleCom_Parser_Fns_Ptr Fn_3, bleCom_Parser_Fns_Ptr Fn_4, bleCom_Parser_Fns_Ptr Fn_5, bleCom_Parser_Fns_Ptr Fn_6, bleCom_Parser_Fns_Ptr Fn_7, bleCom_Parser_Fns_Ptr Fn_8, bleCom_Parser_Fns_Ptr Fn_9, bleCom_Parser_Fns_Ptr Fn_10, bleCom_Parser_Fns_Ptr Fn_11){
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

void mtb_General_App_Lunch(Mtb_CurrentApp_t dAppPath){
    switch(dAppPath.GenApp){
    case 0: mtb_Clk_Tim_AppLunch(dAppPath.SpeApp); break;
    case 1: mtb_Msg_App_Lunch(dAppPath.SpeApp); break;
    case 2: mtb_Calendar_App_Lunch(dAppPath.SpeApp); break;
    case 3: mtb_Weather_App_Lunch(dAppPath.SpeApp); break;
    case 4: mtb_Finance_App_Lunch(dAppPath.SpeApp); break;
    case 5: mtb_Sports_App_Lunch(dAppPath.SpeApp); break;
    case 6: mtb_Animations_App_Lunch(dAppPath.SpeApp); break;
    case 7: mtb_Notifications_App_Lunch(dAppPath.SpeApp); break;
    case 8: mtb_Ai_App_Lunch(dAppPath.SpeApp); break;
    case 9: mtb_Audio_Stream_App_Lunch(dAppPath.SpeApp); break;
    case 10: mtb_sMedia_App_Lunch(dAppPath.SpeApp); break;
    case 11: mtb_Miscellanous_App_Lunch(dAppPath.SpeApp); break;
    // case 12: specAppLunch(dSpecApp); break;
    // case 13: specAppLunch(dSpecApp); break;
    // case 14: specAppLunch(dSpecApp); break;
    // case 15: specAppLunch(dSpecApp); break;
    // case 16: specAppLunch(dSpecApp); break;
    default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
    //statusBarNotif.mtb_Scroll_This_Text("COMMAND DOES NOT MENTION ANY APP TO LUNCH.", YELLOW);
    }
}

//********NUMBER 0 */
void mtb_Clk_Tim_AppLunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(classicClock_App); break;
        case 1: mtb_Launch_This_App(pixelAnimClock_App); break;
        case 2: mtb_Launch_This_App(worldClock_App); break;
        case 3: mtb_Launch_This_App(bigClockCalendar_App); break;
        case 4: mtb_Launch_This_App(stopWatch_App); break;
        default: ESP_LOGI(TAG, "No Apps to Lunch.\n"); break;
        }
}

//********NUMBER 1 */
void mtb_Msg_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(googleNews_App); break;
        case 1: mtb_Launch_This_App(rssNewsApp); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 2 */
void mtb_Calendar_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(google_Calendar_App); break;
        case 1: mtb_Launch_This_App(outlook_Calendar_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 3 */
void mtb_Weather_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(openWeather_App); break;
        case 1: mtb_Launch_This_App(openMeteo_App); break;
        case 2: mtb_Launch_This_App(googleWeather_App); break; 

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 4 */
void mtb_Finance_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(finnhub_Stats_App); break;
        case 1: mtb_Launch_This_App(crypto_Stats_App); break; 
        case 2: mtb_Launch_This_App(currencyExchange_App); break;
        case 3: mtb_Launch_This_App(polygonFX_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 5 */
void mtb_Sports_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(liveFootbalScores_App); break;
        // case 1: mtb_Launch_This_App(classicClock_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 6 */
void mtb_Animations_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(studioLight_App); break;   // ABOUT 10KB RAM IS CONSUMED JUST BY HAVING THIS APPLICATION AMONG THE OTHERS
        case 1: mtb_Launch_This_App(worldFlags_App); break;
        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 7 */
void mtb_Notifications_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(apple_Notifications_App); break;
        // case 1: mtb_Launch_This_App(classicClock_App); break;
        // case 2: mtb_Launch_This_App(classicClock_App); break;
        // case 3: mtb_Launch_This_App(classicClock_App); break; 
        // case 4: mtb_Launch_This_App(classicClock_App); break; 
        // case 5: mtb_Launch_This_App(classicClock_App); break;
        // case 6: mtb_Launch_This_App(classicClock_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 8 */
void mtb_Ai_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(chatGPT_App); break;
        // case 1: mtb_Launch_This_App(classicClock_App); break; 
        // case 2: mtb_Launch_This_App(classicClock_App); break; 
        // case 3: mtb_Launch_This_App(classicClock_App); break;
        // case 4: mtb_Launch_This_App(classicClock_App); break; 
        // case 5: mtb_Launch_This_App(classicClock_App); break; 
        // case 6: mtb_Launch_This_App(classicClock_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 9 */
void mtb_Audio_Stream_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(internetRadio_App); break;
        case 1: mtb_Launch_This_App(musicPlayer_App); break;
        case 2: mtb_Launch_This_App(audSpecAnalyzer_App); break;
        case 3: mtb_Launch_This_App(spotify_App); break;
        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 10 */
void mtb_sMedia_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        case 0: mtb_Launch_This_App(exampleWriteTextApp); break;
        case 1: mtb_Launch_This_App(exampleDrawShapes_App); break;
        // case 2: mtb_Launch_This_App(classicClock_App); break; 
        // case 3: mtb_Launch_This_App(classicClock_App); break; 
        // case 4: mtb_Launch_This_App(classicClock_App); break;
        // case 5: mtb_Launch_This_App(classicClock_App); break;
        // case 6: mtb_Launch_This_App(classicClock_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

//********NUMBER 11 */
void mtb_Miscellanous_App_Lunch(uint16_t dAppNumber){
    switch(dAppNumber){
        // case 0: mtb_Launch_This_App(classicClock_App); break;
        // case 1: mtb_Launch_This_App(classicClock_App); break;
        // case 2: mtb_Launch_This_App(classicClock_App); break;
        // case 3: mtb_Launch_This_App(classicClock_App); break; 
        // case 4: mtb_Launch_This_App(classicClock_App); break;
        // case 5: mtb_Launch_This_App(classicClock_App); break;
        // case 6: mtb_Launch_This_App(classicClock_App); break;

        default: ESP_LOGI(TAG, "No Apps to Lunch.\n");
            break;
    }
}

esp_err_t mtb_Write_Nvs_Struct(const char* keyIdentifier, void* struct_pointer, size_t struct_sized) {
    NvsAccessParams_t dataWriteHolder{
        .read_OR_Write = NVS_MEM_WRITE,
        .key = keyIdentifier,
        .struct_ptr = struct_pointer,
        .struct_size = struct_sized
    };
    xQueueSend(nvsAccessQueue, &dataWriteHolder, pdMS_TO_TICKS(5000));
    mtb_Start_This_Service(read_Write_NVS_Sv);
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
    mtb_Start_This_Service(read_Write_NVS_Sv);
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