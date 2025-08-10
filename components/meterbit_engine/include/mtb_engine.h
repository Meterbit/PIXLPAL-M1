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

enum do_Prev_App_t{
    SUSPEND_PREVIOUS_APP = 1,
    DESTROY_PREVIOUS_APP,
    IGNORE_PREVIOUS_APP,
};

struct CurrentApp_t{
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

extern CurrentApp_t currentApp;
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
extern String unixTimeToReadable(time_t unixTime, int timezoneOffsetHours = 0);
extern String formatLargeNumber(double number);
extern String formatIsoDate(const String& isoDateTime);
extern String formatIsoTime(const String& isoDateTime);
extern String formatDateFromTimestamp(time_t timestamp);
extern String formatTimeFromTimestamp(time_t timestamp);
extern String urlencode(const char* str);
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

// Define the document type
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;


typedef void (*encoderFn_ptr_t)(rotary_encoder_rotation_t);
typedef void (*buttonFn_ptr_t)(button_event_t);

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

class Service_With_Fns : public Mtb_Services{
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

        Service_With_Fns();
        Service_With_Fns(void (*dService)(void *), TaskHandle_t *dServiceHandle_ptr, const char *dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServicePSRamStack = pdFALSE, uint8_t dServiceCore = 0) : Mtb_Services(dService, dServiceHandle_ptr,dServiceName, dStackSize, dServicePriority, dServicePSRamStack, dServiceCore){}
};

class Mtb_Applications{
public:
    void (*application)(void *);        // This is the task which can be created or deleted and any given time.
    char appName[50] = {0};             // Name of the application task.
    uint32_t stackSize;                 // Stack size of the application
    uint8_t appPriority;                // Priority of the application
    TaskHandle_t* appHandle_ptr;        // Pointer to the application task handle.
    uint8_t appCore;                    // Core on which the application task is running on.
    BaseType_t usePSRAM_Stack;                // Create task stack in PSRAM (Only use for task that don't require speed)
    StackType_t *task_stack = NULL;
    StaticTask_t* tcb_psram = NULL;

    Mtb_Services* appServices[10] = {nullptr};  // An array of 10 Service Pointers. This will hold pointers to the Mtb_Services tasks both generic and perculiar. e.g. Mic Service 
    void (*mtb_App_EncoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing;
    void (*mtb_App_ButtonFn_ptr)(button_event_t) = buttonDoNothing;
    void *parameters;
    bool fullScreen = false; 
    bool elementRefresh;    // Refresh the various elements/components displayed onscreen by the app.
    uint8_t app_is_Running = pdFALSE;
    uint8_t showStatusBarClock = pdFALSE;

    do_Prev_App_t action_On_Prev_App = DESTROY_PREVIOUS_APP;

    static  Mtb_Applications *otaAppHolder;
    static  Mtb_Applications *currentRunningApp;
    static Mtb_Applications *previousRunningApp;
    static bool internetConnectStatus;
    static bool usbPenDriveConnectStatus;
    static bool usbPenDriveMounted;
    static bool pxpWifiConnectStatus;
    static bool bleAdvertisingStatus;
    static bool bleCentralContd;
    // static bool mqttPhoneConnectStatus;
    static uint8_t firmwareOTA_Status;
    static uint8_t spiffsOTA_Status;
    //void *app_Dyn_Mems[5] = {nullptr};

    bool appRunner();               // The function that runs any selected.
    //bool appRunner(void*);          // The function that runs any selected.
    static void appResume(Mtb_Applications *);           // The function that resumes the app after being suspended.
    static void appSuspend(Mtb_Applications *);
    static void appDestroy(Mtb_Applications *);
    static void actionOnPreviousApp(do_Prev_App_t);

    // Overload the new operator
    void* operator new(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    // Overload the delete operator
    void operator delete(void* ptr) {
        heap_caps_free(ptr);
    }

    Mtb_Applications();
    Mtb_Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, uint32_t dStackSize, uint8_t psRamStack, uint8_t core);
    //virtual 
};

class Applications_FullScreen : public Mtb_Applications{
    public:
        Applications_FullScreen();
        Applications_FullScreen(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = true;
        }
};

class Mtb_Applications_StatusBar : public Mtb_Applications{
    public:
        Mtb_Applications_StatusBar();
        Mtb_Applications_StatusBar(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = false;
        }
};

extern void mtb_App_Init(Mtb_Applications*, Mtb_Services* pointer_0 = nullptr, Mtb_Services* pointer_1 = nullptr,Mtb_Services* pointer_2 = nullptr,
                                 Mtb_Services* pointer_3 = nullptr, Mtb_Services* pointer_4 = nullptr, Mtb_Services* pointer_5 = nullptr,
                                 Mtb_Services* pointer_6 = nullptr, Mtb_Services* pointer_7 = nullptr, Mtb_Services* pointer_8 = nullptr, 
                                Mtb_Services* pointer_9 = nullptr);



// App Parser Functions
extern void mtb_Launch_This_App(Mtb_Applications* dApp, do_Prev_App_t do_Prv_App = DESTROY_PREVIOUS_APP);
extern void start_This_Service(Mtb_Services*);
extern void resume_This_Service(Mtb_Services*);
extern void suspend_This_Service(Mtb_Services*);
extern void mtb_End_This_Service(Mtb_Services *);
extern void mtb_End_This_App(Mtb_Applications *);

extern void mtb_General_App_Lunch(CurrentApp_t);

// Supporting Apps and Tasks
extern TaskHandle_t statusBarClock_H;
extern void statusBarClock(void*);

// All Apps Categories
extern void clk_Tim_AppLunch(uint16_t);
extern void msgAppLunch(uint16_t);
extern void calendarAppLunch(uint16_t);
extern void weatherAppLunch(uint16_t);
extern void sportsAppLunch(uint16_t);
extern void animationsAppLunch(uint16_t);
extern void financeAppLunch(uint16_t);
extern void sMediaAppLunch(uint16_t);
extern void notificationsAppLunch(uint16_t);
extern void ai_AppLunch(uint16_t);
extern void audioStreamAppLunch(uint16_t);
extern void miscellanousAppLunch(uint16_t);

// System Sevices
extern Service_With_Fns* mtb_Ble_AppComm_Parser_Sv;
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

extern Service_With_Fns* encoder_Task_Sv;       // USES PSRAM AS TASK STACK
extern Service_With_Fns* button_Task_Sv;        // USES PSRAM AS TASK STACK
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
extern Applications_FullScreen* firmwareUpdate_App;
extern Applications_FullScreen* otaUpdateApplication_App;

// All User Apps
// Clocks and Timers
extern Mtb_Applications_StatusBar* classicClock_App;            // App Communication Route: 0/0
extern Applications_FullScreen* pixelAnimClock_App;         // App Communication Route: 0/1
extern Mtb_Applications_StatusBar* worldClock_App;              // App Communication Route: 0/2
extern Applications_FullScreen* bigClockCalendar_App;        // App Communication Route: 0/3
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
extern Applications_FullScreen *studioLight_App;             // App Communication Route: 6/0
extern Applications_FullScreen *worldFlags_App;               // App Communication Route: 6/1

// Notifications
extern Mtb_Applications_StatusBar *apple_Notifications_App;     // App Communication Route: 7/0

// Artificial Intelligence
extern Mtb_Applications_StatusBar *chatGPT_App;                 // App Communication Route: 8/0

// Audio and Media
extern Mtb_Applications_StatusBar* internetRadio_App;           // App Communication Route: 9/0
extern Mtb_Applications_StatusBar* musicPlayer_App;             // App Communication Route: 9/1
extern Applications_FullScreen* audSpecAnalyzer_App;        // App Communication Route: 9/2
extern Mtb_Applications_StatusBar *spotify_App;                 // App Communication Route: 9/3
#endif