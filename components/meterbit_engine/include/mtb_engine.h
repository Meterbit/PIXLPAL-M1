#ifndef APPLICATIONS_H
#define APPLICATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "encoder.h"
#include "button.h"
#include "mtb_ble.h"
#include "Arduino.h"
#include "ArduinoJson.h"
#include "mtb_nvs.h"
#include "mtb_graphics.h"
#include "esp_heap_caps.h"

#define APPS_PARSER_QUEUE_SIZE 5
#define NVS_MEM_READ            0
#define NVS_MEM_WRITE           1

#define MTB_APP_IS_ACTIVE (thisApp->app_is_Running)
#define MTB_SERV_IS_ACTIVE (thisServ->service_is_Running)

extern QueueHandle_t clock_Update_Q;
extern TaskHandle_t appLuncher_Task_H;
extern void appLuncherTask(void *);
extern void freeServAndAppPSRAM_Task(void *);
extern void nvsAccessTask(void *);
extern QueueHandle_t appLuncherQueue;
extern QueueHandle_t freeServAndAppPSRAM_Q;
extern QueueHandle_t nvsAccessQueue;
extern SemaphoreHandle_t nvsAccessComplete_Sem;
extern QueueHandle_t running_App_BLECom_Queue;
//extern TimerHandle_t bleRestoreTimer_H;

enum Mtb_Do_Prev_App_t{
    SUSPEND_PREVIOUS_APP = 1,
    DESTROY_PREVIOUS_APP,
    IGNORE_PREVIOUS_APP,
};

struct Mtb_CurrentApp_t{
    uint16_t GenApp;
    uint16_t SpeApp;
};

struct NvsAccessParams_t{
  bool read_OR_Write;
  const char* key;
  void* struct_ptr;
  size_t struct_size;
};
//**************************************************************************************************************************

extern Mtb_CurrentApp_t currentApp;
extern esp_err_t mtb_Read_Nvs_Struct(const char* key, void* struct_ptr, size_t struct_size);
extern esp_err_t mtb_Write_Nvs_Struct(const char *key, void *struct_ptr, size_t struct_size);
extern void (*encoderFn_ptr)(rotary_encoder_rotation_t);
extern void (*buttonFn_ptr)(button_event_t);
extern void encoderDoNothing(rotary_encoder_rotation_t);
extern void buttonDoNothing(button_event_t);

extern void mtb_Brightness_Control(rotary_encoder_rotation_t);
extern void mtb_Vol_Control_Encoder(rotary_encoder_rotation_t);
extern void randomButtonControl(button_event_t);

// GENERAL UTILITIES FUNCTIONS
extern String mtb_Unix_Time_To_Readable(time_t unixTime, int timezoneOffsetHours = 0);
extern String mtb_Format_Large_Number(double number);
extern String mtb_Url_Encode(const char* str);
extern String mtb_Format_Iso_Date(const String& isoDateTime);
extern String mtb_Format_Iso_Time(const String& isoDateTime);
extern String mtb_Format_Date_From_Timestamp(time_t timestamp);
extern String mtb_Format_Time_From_Timestamp(time_t timestamp);
extern String mtb_Get_Current_Time_RFC3339(void);


// Custom allocator using PSRAM
struct SpiRamAllocator {
  void* allocate(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  }
  void deallocate(void* pointer) {
    heap_caps_free(pointer);
  }
  void* reallocate(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

// Use the SpiRamDocument type for large JSON documents that will be allocated in PSRAM
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

typedef void (*encoderFn_ptr_t)(rotary_encoder_rotation_t);
typedef void (*buttonFn_ptr_t)(button_event_t);

// Services Class
class Mtb_Services {
public:
void (*service)(void *) = nullptr;         // This is the task which can be created or deleted and any given time.
char serviceName[50] = {0};                // Name of the application task.
uint32_t stackSize = 2048;                 // Stack size of the application
uint8_t servicePriority = 1;               // Priority of the application
TaskHandle_t* serviceT_Handle_ptr = NULL;  // Pointer to the Service's task handle.
uint8_t serviceCore = 0;                   // Core on which the application task is running on.
BaseType_t usePSRAM_Stack = pdFALSE;       // Create task stack in PSRAM (Only use for task that don't require speed)
StackType_t *task_stack = NULL;
StaticTask_t* tcb_psram = NULL;
//void* serv_Dyn_Mems[5] = {nullptr};
uint8_t service_is_Running = pdFALSE;

// Overload the new operator
void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

// Overload the delete operator
void operator delete(void* ptr) {heap_caps_free(ptr);}

// Services Constructors
Mtb_Services(){};
Mtb_Services(void (*dService)(void *), TaskHandle_t* dServiceHandle_ptr, const char* dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServicePSRamStack = pdFALSE, uint8_t dServiceCore = 0){
    service = dService;
    serviceT_Handle_ptr = dServiceHandle_ptr;
    strcpy(serviceName, dServiceName);
    stackSize = dStackSize;
    servicePriority = dServicePriority;
    serviceCore = dServiceCore;
    usePSRAM_Stack = dServicePSRamStack;
}
};

using bleCom_Parser_Fns_Ptr = void (*)(JsonDocument&);     // Defining the signature of a function pointer.


// Services with Functions Class.
class Mtb_Service_With_Fns : public Mtb_Services{
    public:
        bleCom_Parser_Fns_Ptr bleAppComServiceFns[12] = {nullptr};
        void mtb_Register_Ble_Comm_ServiceFns(bleCom_Parser_Fns_Ptr Fn_0, bleCom_Parser_Fns_Ptr Fn_1 = nullptr, bleCom_Parser_Fns_Ptr Fn_2 = nullptr, bleCom_Parser_Fns_Ptr Fn_3 = nullptr, bleCom_Parser_Fns_Ptr Fn_4 = nullptr, bleCom_Parser_Fns_Ptr Fn_5 = nullptr, bleCom_Parser_Fns_Ptr Fn_6 = nullptr, bleCom_Parser_Fns_Ptr Fn_7 = nullptr, bleCom_Parser_Fns_Ptr Fn_8 = nullptr, bleCom_Parser_Fns_Ptr Fn_9 = nullptr, bleCom_Parser_Fns_Ptr Fn_10 = nullptr, bleCom_Parser_Fns_Ptr Fn_11 = nullptr);
    // Overload the new operator
    void* operator new(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    // Overload the delete operator
    void operator delete(void* ptr) {
        heap_caps_free(ptr);
    }
        // Services with Functions Constructors
        Mtb_Service_With_Fns();
        Mtb_Service_With_Fns(void (*dService)(void *), TaskHandle_t *dServiceHandle_ptr, const char *dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServicePSRamStack = pdFALSE, uint8_t dServiceCore = 0) : Mtb_Services(dService, dServiceHandle_ptr,dServiceName, dStackSize, dServicePriority, dServicePSRamStack, dServiceCore){}
};


// The Applications Class
class Mtb_Applications{
public:
    void (*application)(void *);        // This is the task which can be created or deleted at any given time.
    char appName[50] = {0};             // Name of the application task.
    uint32_t stackSize;                 // Stack size of the application
    uint8_t appPriority;                // Priority of the application
    TaskHandle_t* appHandle_ptr;        // Pointer to the application task handle.
    uint8_t appCore;                    // Core on which the application task is running on.
    BaseType_t usePSRAM_Stack;          // Create task stack in PSRAM (Only use for task that don't require speed, or for tasks that require large stack size, or tasks that don't read flash using LittleFS)
    StackType_t *task_stack = NULL;     // Pointer to the task stack, if usePSRAM_Stack is true, this will be allocated in PSRAM.
    StaticTask_t* tcb_psram = NULL;     // Pointer to the task control block, if usePSRAM_Stack is true, this will be allocated in PSRAM.

    Mtb_Services* appServices[10] = {nullptr};  // An array of 10 Service Pointers. This will hold pointers to the Mtb_Services tasks both generic and perculiar. e.g. Mic Service 
    void (*mtb_App_EncoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing;        // Pointer to the function that will be called when the rotary encoder is rotated.
    void (*mtb_App_ButtonFn_ptr)(button_event_t) = buttonDoNothing;                     // Pointer to the function that will be called when the button is pressed.
    void *parameters;                                               // Pointer to the parameters that will be passed to the application task.
    bool fullScreen = false;                                        // If true, the application will run in full screen mode, otherwise it will run in status bar mode.
    bool elementRefresh;                                            // Refresh the various elements/components displayed onscreen by the app.
    uint8_t app_is_Running = pdFALSE;                               // This is used to check if the application is running or not. It is set to pdTRUE when the application is running, and pdFALSE when it is not running.
    uint8_t showStatusBarClock = pdFALSE;                           // This is used to check if the status bar clock should be shown or not. It is set to pdTRUE when the status bar clock should be shown, and pdFALSE when it should not be shown.

    Mtb_Do_Prev_App_t action_On_Prev_App = DESTROY_PREVIOUS_APP;    // This is used to check what action should be taken on the previous application when a new application is launched. It can be set to SUSPEND_PREVIOUS_APP, DESTROY_PREVIOUS_APP, or IGNORE_PREVIOUS_APP.

    static  Mtb_Applications *otaAppHolder;                         // This is used to hold the OTA application when it is launched. It is used to resume the OTA application after it has been suspended.
    static  Mtb_Applications *currentRunningApp;                    // This is used to hold the current running application. It is used to resume the application after it has been suspended.
    static Mtb_Applications *previousRunningApp;                    // This is used to hold the previous running application. It is used to destroy the previous application when a new application is launched.
    static bool internetConnectStatus;                              // This is used to check if the device is connected to the internet or not. It is set to true when the device is connected to the internet, and false when it is not connected.
    static bool usbPenDriveConnectStatus;                           // This is used to check if the USB pen drive is connected or not. It is set to true when the USB pen drive is connected, and false when it is not connected.
    static bool usbPenDriveMounted;                                 // This is used to check if the USB pen drive is mounted or not. It is set to true when the USB pen drive is mounted, and false when it is not mounted.
    static bool pxpWifiConnectStatus;                               // This is used to check if the PXP WiFi is connected or not. It is set to true when the PXP WiFi is connected, and false when it is not connected.
    static bool bleAdvertisingStatus;                               // This is used to check if the BLE advertising is enabled or not. It is set to true when the BLE advertising is enabled, and false when it is not enabled.
    static bool bleCentralContd;                                    // This is used to check if the BLE central is connected or not. It is set to true when the BLE central is connected, and false when it is not connected.
    // static bool mqttPhoneConnectStatus;
    static uint8_t firmwareOTA_Status;                              // This is used to check the status of the firmware OTA update. It is set to 6 when the OTA update is not started, and it can be set to other values to indicate the status of the OTA update.
    static uint8_t spiffsOTA_Status;                                // This is used to check the status of the SPIFFS OTA update. It is set to 6 when the OTA update is not started, and it can be set to other values to indicate the status of the OTA update.
    //void *app_Dyn_Mems[5] = {nullptr};

    bool appRunner();                                               // The function that runs any selected application. It creates the task with the application function, name, stack size, priority, and core.     
    //bool appRunner(void*);                                        // The function that runs any selected.
    static void appResume(Mtb_Applications *);                      // The function that resumes the app after being suspended.
    static void appSuspend(Mtb_Applications *);                     // The function that suspends the app.
    static void appDestroy(Mtb_Applications *);
    static void actionOnPreviousApp(Mtb_Do_Prev_App_t);             // This function is used to take action on the previous application when a new application is launched. It can be set to SUSPEND_PREVIOUS_APP, DESTROY_PREVIOUS_APP, or IGNORE_PREVIOUS_APP.

    // Overload the new operator
    void* operator new(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);           // Allocate memory in PSRAM
    }

    // Overload the delete operator
    void operator delete(void* ptr) {
        heap_caps_free(ptr);                                        // Free memory allocated in PSRAM
    }

    // Applications Constructors
    Mtb_Applications();                                             // Default constructor
    Mtb_Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, uint32_t dStackSize, uint8_t psRamStack, uint8_t core);
    //virtual 
};


// Full Screen Applications and Status Bar Applications
class Mtb_Applications_FullScreen : public Mtb_Applications{            
    public:
        Mtb_Applications_FullScreen();
        Mtb_Applications_FullScreen(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = true;
        }
};


// Status Bar Applications
class Mtb_Applications_StatusBar : public Mtb_Applications{
    public:
        Mtb_Applications_StatusBar();
        Mtb_Applications_StatusBar(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = false;
        }
};


//*********************************************************************************** */
// Mtb_Applications Init Function
extern void mtb_App_Init(Mtb_Applications*, Mtb_Services* pointer_0 = nullptr, Mtb_Services* pointer_1 = nullptr,Mtb_Services* pointer_2 = nullptr,
                                 Mtb_Services* pointer_3 = nullptr, Mtb_Services* pointer_4 = nullptr, Mtb_Services* pointer_5 = nullptr,
                                 Mtb_Services* pointer_6 = nullptr, Mtb_Services* pointer_7 = nullptr, Mtb_Services* pointer_8 = nullptr, 
                                Mtb_Services* pointer_9 = nullptr);



// App Parser Functions
extern void mtb_Launch_This_App(Mtb_Applications* dApp, Mtb_Do_Prev_App_t do_Prv_App = DESTROY_PREVIOUS_APP);
extern void mtb_Start_This_Service(Mtb_Services*);
extern void mtb_Resume_This_Service(Mtb_Services*);
extern void mtb_Suspend_This_Service(Mtb_Services*);
extern void mtb_End_This_Service(Mtb_Services *);
extern void mtb_End_This_App(Mtb_Applications *);

extern void mtb_General_App_Lunch(Mtb_CurrentApp_t);

// Supporting Apps and Tasks
extern TaskHandle_t statusBarClock_H;
extern void mtb_StatusBar_Clock(void*);

// All Apps Categories
extern void mtb_Clk_Tim_AppLunch(uint16_t);
extern void mtb_Msg_App_Lunch(uint16_t);
extern void mtb_Calendar_App_Lunch(uint16_t);
extern void mtb_Weather_App_Lunch(uint16_t);
extern void mtb_Sports_App_Lunch(uint16_t);
extern void mtb_Animations_App_Lunch(uint16_t);
extern void mtb_Finance_App_Lunch(uint16_t);
extern void mtb_sMedia_App_Lunch(uint16_t);
extern void mtb_Notifications_App_Lunch(uint16_t);
extern void mtb_Ai_App_Lunch(uint16_t);
extern void mtb_Audio_Stream_App_Lunch(uint16_t);
extern void mtb_Miscellanous_App_Lunch(uint16_t);

// System Sevices
extern Mtb_Service_With_Fns* mtb_Ble_AppComm_Parser_Sv;
extern Mtb_Services* ble_SetCom_Parse_Sv;             // USES PSRAM AS TASK STACK
extern Mtb_Services* beep_Buzzer_Sv;                // USES PSRAM AS TASK STACK
//extern Mtb_Services* statusBarIconUpdate_Sv;        // USES PSRAM AS TASK STACK
extern Mtb_Services* sntp_Time_Sv;                  
extern Mtb_Services* app_Luncher_Task_Sv;           // USES PSRAM AS TASK STACK
extern Mtb_Services* scroll_Tasks_Sv[];             // USES PSRAM AS TASK STACK
extern Mtb_Services* read_Write_NVS_Sv;
extern Mtb_Services* pngLocalImageDrawer_Sv;
extern Mtb_Services* pngOnlineImageDrawer_Sv;       // USES PSRAM AS TASK STACK
extern Mtb_Services* svgOnlineImageDrawer_Sv;       // USES PSRAM AS TASK STACK
extern Mtb_Services *gitHubFileDwnload_Sv;

extern Mtb_Services* mtb_AudioOut_Sv;
extern Mtb_Services* mtb_Mic_Sv;
// extern Mtb_Services* usb_Speaker_Sv;
// extern Mtb_Services* bleControl_Sv;

extern Mtb_Service_With_Fns* encoder_Task_Sv;       // USES PSRAM AS TASK STACK
extern Mtb_Service_With_Fns* button_Task_Sv;        // USES PSRAM AS TASK STACK
extern Mtb_Services* usb_Mass_Storage_Sv;           //
extern Mtb_Services* freeServAndAppPSRAM_Sv;        //


//*********************************************************************************** */
// Mtb_Applications SECTION (USERS AND SYSTEM APPS)

// Application Mtb_Services
extern Mtb_Services* pixAnimClkGif_Sv;
extern Mtb_Services* spotifyScreenUpdate_Sv;        // USES PSRAM AS TASK STACK
extern Mtb_Services* Audio_Listening_Sv;
extern Mtb_Services* mtb_Status_Bar_Clock_Sv;             // USES PSRAM AS TASK STACK

// All System Apps
extern Mtb_Applications_FullScreen* firmwareUpdate_App;
extern Mtb_Applications_FullScreen* otaUpdateApplication_App;

// All User Apps
// Clocks and Timers
extern Mtb_Applications_StatusBar* classicClock_App;            // App Communication Route: 0/0
extern Mtb_Applications_FullScreen* pixelAnimClock_App;         // App Communication Route: 0/1
extern Mtb_Applications_StatusBar* worldClock_App;              // App Communication Route: 0/2
extern Mtb_Applications_FullScreen* bigClockCalendar_App;        // App Communication Route: 0/3
extern Mtb_Applications_StatusBar* stopWatch_App;               // App Communication Route: 0/4

// News and Messages
extern Mtb_Applications_StatusBar *googleNews_App;              // App Communication Route: 1/0 .... Replace this with google news app. Find api in Rapid API.
extern Mtb_Applications_StatusBar *rssNewsApp;                  // App Communication Route: 1/1

// Calendars
extern Mtb_Applications_StatusBar *google_Calendar_App;         // App Communication Route: 2/0
extern Mtb_Applications_StatusBar *outlook_Calendar_App;        // App Communication Route: 2/1

// Weather Updates
extern Mtb_Applications_StatusBar *openWeather_App;             // App Communication Route: 3/0
extern Mtb_Applications_StatusBar *openMeteo_App;               // App Communication Route: 3/1
extern Mtb_Applications_StatusBar *googleWeather_App;           // App Communication Route: 3/2

// Finance
extern Mtb_Applications_StatusBar *finnhub_Stats_App;          // App Communication Route: 4/0
extern Mtb_Applications_StatusBar *crypto_Stats_App;            // App Communication Route: 4/1
extern Mtb_Applications_StatusBar *currencyExchange_App;        // App Communication Route: 4/2
extern Mtb_Applications_StatusBar *polygonFX_App;              // App Communication Route: 4/3

// Sports
extern Mtb_Applications_StatusBar *liveFootbalScores_App;       // App Communication Route: 5/0

// Animations
extern Mtb_Applications_FullScreen *studioLight_App;             // App Communication Route: 6/0
extern Mtb_Applications_FullScreen *worldFlags_App;               // App Communication Route: 6/1

// Notifications
extern Mtb_Applications_StatusBar *apple_Notifications_App;     // App Communication Route: 7/0

// Artificial Intelligence
extern Mtb_Applications_StatusBar *chatGPT_App;                 // App Communication Route: 8/0

// Audio and Media
extern Mtb_Applications_StatusBar* internetRadio_App;           // App Communication Route: 9/0
extern Mtb_Applications_StatusBar* musicPlayer_App;             // App Communication Route: 9/1
extern Mtb_Applications_FullScreen* audSpecAnalyzer_App;        // App Communication Route: 9/2
extern Mtb_Applications_StatusBar *spotify_App;                 // App Communication Route: 9/3

extern Mtb_Applications_FullScreen* exampleWriteTextApp;                  // App Communication Route: 10/0
#endif