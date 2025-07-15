#ifndef APPLICATIONS_H
#define APPLICATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "encoder.h"
#include "button.h"
#include "mtbBLE_Control.h"
#include "Arduino.h"
#include "ArduinoJson.h"
#include "nvsMem.h"
#include "ledPanel.h"
#include "esp_heap_caps.h"

#define APPS_PARSER_QUEUE_SIZE 5
#define NVS_MEM_READ            0
#define NVS_MEM_WRITE           1

#define THIS_APP_IS_ACTIVE (thisApp->app_is_Running)
#define THIS_SERV_IS_ACTIVE (thisServ->service_is_Running)

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
extern esp_err_t read_struct_from_nvs(const char* key, void* struct_ptr, size_t struct_size);
extern esp_err_t write_struct_to_nvs(const char *key, void *struct_ptr, size_t struct_size);
extern void (*encoderFn_ptr)(rotary_encoder_rotation_t);
extern void (*buttonFn_ptr)(button_event_t);
extern void encoderDoNothing(rotary_encoder_rotation_t);
extern void buttonDoNothing(button_event_t);

extern void brightnessControl(rotary_encoder_rotation_t);
extern void volumeControl_Encoder(rotary_encoder_rotation_t);
extern void randomButtonControl(button_event_t);

// GENERAL UTILITIES FUNCTIONS
extern String unixTimeToReadable(time_t unixTime, int timezoneOffsetHours = 0);
extern String formatLargeNumber(double number);
extern String formatIsoDate(const String& isoDateTime);
extern String formatIsoTime(const String& isoDateTime);
extern String formatDateFromTimestamp(time_t timestamp);
extern String formatTimeFromTimestamp(time_t timestamp);
extern String urlencode(const char* str);
extern String getCurrentTimeRFC3339(void);


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

class Services {
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

Services(){};
Services(void (*dService)(void *), TaskHandle_t* dServiceHandle_ptr, const char* dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServicePSRamStack = pdFALSE, uint8_t dServiceCore = 0){
    service = dService;
    serviceT_Handle_ptr = dServiceHandle_ptr;
    strcpy(serviceName, dServiceName);
    stackSize = dStackSize;
    servicePriority = dServicePriority;
    serviceCore = dServiceCore;
    usePSRAM_Stack = dServicePSRamStack;
}
};

using bleCom_Parser_Fns_Ptr = void (*)(DynamicJsonDocument&);     // Defining the signature of a function pointer.

class Service_With_Fns : public Services{
    public:
        bleCom_Parser_Fns_Ptr bleAppComServiceFns[12] = {nullptr};
        void register_BLE_Com_ServiceFns(bleCom_Parser_Fns_Ptr Fn_0, bleCom_Parser_Fns_Ptr Fn_1 = nullptr, bleCom_Parser_Fns_Ptr Fn_2 = nullptr, bleCom_Parser_Fns_Ptr Fn_3 = nullptr, bleCom_Parser_Fns_Ptr Fn_4 = nullptr, bleCom_Parser_Fns_Ptr Fn_5 = nullptr, bleCom_Parser_Fns_Ptr Fn_6 = nullptr, bleCom_Parser_Fns_Ptr Fn_7 = nullptr, bleCom_Parser_Fns_Ptr Fn_8 = nullptr, bleCom_Parser_Fns_Ptr Fn_9 = nullptr, bleCom_Parser_Fns_Ptr Fn_10 = nullptr, bleCom_Parser_Fns_Ptr Fn_11 = nullptr);
    // Overload the new operator
    void* operator new(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    // Overload the delete operator
    void operator delete(void* ptr) {
        heap_caps_free(ptr);
    }

        Service_With_Fns();
        Service_With_Fns(void (*dService)(void *), TaskHandle_t *dServiceHandle_ptr, const char *dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServicePSRamStack = pdFALSE, uint8_t dServiceCore = 0) : Services(dService, dServiceHandle_ptr,dServiceName, dStackSize, dServicePriority, dServicePSRamStack, dServiceCore){}
};

class Applications{
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

    Services* appServices[10] = {nullptr};  // An array of 10 Service Pointers. This will hold pointers to the Services tasks both generic and perculiar. e.g. Mic Service 
    void (*app_EncoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing;
    void (*app_ButtonFn_ptr)(button_event_t) = buttonDoNothing;
    void *parameters;
    bool fullScreen = false; 
    bool elementRefresh;    // Refresh the various elements/components displayed onscreen by the app.
    uint8_t app_is_Running = pdFALSE;
    uint8_t showStatusBarClock = pdFALSE;

    do_Prev_App_t action_On_Prev_App = DESTROY_PREVIOUS_APP;

    static  Applications *otaAppHolder;
    static  Applications *currentRunningApp;
    static Applications *previousRunningApp;
    static bool internetConnectStatus;
    static bool pxpWifiConnectStatus;
    static bool bleAdvertisingStatus;
    static bool bleCentralContd;
    // static bool mqttPhoneConnectStatus;
    static uint8_t firmwareOTA_Status;
    //void *app_Dyn_Mems[5] = {nullptr};

    bool appRunner();               // The function that runs any selected.
    //bool appRunner(void*);          // The function that runs any selected.
    static void appResume(Applications *);           // The function that resumes the app after being suspended.
    static void appSuspend(Applications *);
    static void appDestroy(Applications *);
    static void actionOnPreviousApp(do_Prev_App_t);

    // Overload the new operator
    void* operator new(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }

    // Overload the delete operator
    void operator delete(void* ptr) {
        heap_caps_free(ptr);
    }

    Applications();
    Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, uint32_t dStackSize, uint8_t psRamStack, uint8_t core);
    //virtual 
};

class Applications_FullScreen : public Applications{
    public:
        Applications_FullScreen();
        Applications_FullScreen(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = true;
        }
};

class Applications_StatusBar : public Applications{
    public:
        Applications_StatusBar();
        Applications_StatusBar(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, uint32_t dStackSize = 4096, uint8_t psRamStack = pdFALSE, uint8_t core = 0) : 
        Applications(dApplication, dAppHandle_ptr, dAppName, dStackSize, psRamStack, core) { 
            fullScreen = false;
        }
};

extern void appsInitialization(Applications*, Services* pointer_0 = nullptr, Services* pointer_1 = nullptr,Services* pointer_2 = nullptr,
                                 Services* pointer_3 = nullptr, Services* pointer_4 = nullptr, Services* pointer_5 = nullptr,
                                 Services* pointer_6 = nullptr, Services* pointer_7 = nullptr, Services* pointer_8 = nullptr, 
                                Services* pointer_9 = nullptr);



// App Parser Functions
extern void launchThisApp(Applications* dApp, do_Prev_App_t do_Prv_App = DESTROY_PREVIOUS_APP);
extern void start_This_Service(Services*);
extern void resume_This_Service(Services*);
extern void suspend_This_Service(Services*);
extern void kill_This_Service(Services *);
extern void kill_This_App(Applications *);

extern void generalAppLunch(CurrentApp_t);

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
extern Service_With_Fns* ble_AppCom_Parser_Sv;
extern Services* ble_SetCom_Parse_Sv;             // USES PSRAM AS TASK STACK
extern Services* beep_Buzzer_Sv;                // USES PSRAM AS TASK STACK
//extern Services* statusBarIconUpdate_Sv;        // USES PSRAM AS TASK STACK
extern Services* sntp_Time_Sv;                  
extern Services* app_Luncher_Task_Sv;           // USES PSRAM AS TASK STACK
extern Services* scroll_Tasks_Sv[];             // USES PSRAM AS TASK STACK
extern Services* read_Write_NVS_Sv;
extern Services* pngLocalImageDrawer_Sv;
extern Services* pngOnlineImageDrawer_Sv;       // USES PSRAM AS TASK STACK
extern Services* svgOnlineImageDrawer_Sv;       // USES PSRAM AS TASK STACK
extern Services *gitHubFileDwnload_Sv;

extern Services* audioOutProcessing_Sv;
extern Services* micInProcessing_Sv;
// extern Services* usb_Speaker_Sv;
// extern Services* bleControl_Sv;

extern Service_With_Fns* encoder_Task_Sv;       // USES PSRAM AS TASK STACK
extern Service_With_Fns* button_Task_Sv;        // USES PSRAM AS TASK STACK
extern Services* usb_Mass_Storage_Sv;

extern Services* freeServAndAppPSRAM_Sv;


//*********************************************************************************** */
// APPLICATIONS SECTION (USERS AND SYSTEM APPS)

// Application Services
extern Services* pixAnimClkGif_Sv;
extern Services* spotifyScreenUpdate_Sv;        // USES PSRAM AS TASK STACK
extern Services* Audio_Listening_Sv;
extern Services* statusBarClock_Sv;             // USES PSRAM AS TASK STACK

// All System Apps
extern Applications_FullScreen* firmwareUpdate_App;
extern Applications_FullScreen* otaUpdateApplication_App;

// All User Apps
// Clocks and Timers
extern Applications_StatusBar* classicClock_App;            // App Communication Route: 0/0
extern Applications_FullScreen* pixelAnimClock_App;         // App Communication Route: 0/1
extern Applications_StatusBar* worldClock_App;              // App Communication Route: 0/2
extern Applications_FullScreen* bigClockCalendar_App;        // App Communication Route: 0/3
extern Applications_StatusBar* stopWatch_App;               // App Communication Route: 0/4

// News and Messages
extern Applications_StatusBar *googleNews_App;              // App Communication Route: 1/0 .... Replace this with google news app. Find api in Rapid API.
extern Applications_StatusBar *rssNewsApp;                  // App Communication Route: 1/1

// Calendars
extern Applications_StatusBar *google_Calendar_App;         // App Communication Route: 2/0
extern Applications_StatusBar *outlook_Calendar_App;        // App Communication Route: 2/1

// Weather Updates
extern Applications_StatusBar *openWeather_App;             // App Communication Route: 3/0
extern Applications_StatusBar *openMeteo_App;               // App Communication Route: 3/1
extern Applications_StatusBar *googleWeather_App;           // App Communication Route: 3/2

// Finance
extern Applications_StatusBar *finnhub_Stats_App;          // App Communication Route: 4/0
extern Applications_StatusBar *crypto_Stats_App;            // App Communication Route: 4/1
extern Applications_StatusBar *currencyExchange_App;        // App Communication Route: 4/2
extern Applications_StatusBar *polygonFX_App;              // App Communication Route: 4/3

// Sports
extern Applications_StatusBar *liveFootbalScores_App;       // App Communication Route: 5/0

// Animations
extern Applications_FullScreen *studioLight_App;             // App Communication Route: 6/0
extern Applications_FullScreen *worldFlags_App;               // App Communication Route: 6/1

// Notifications
extern Applications_StatusBar *apple_Notifications_App;     // App Communication Route: 7/0

// Artificial Intelligence
extern Applications_StatusBar *chatGPT_App;                 // App Communication Route: 8/0

// Audio and Media
extern Applications_StatusBar* internetRadio_App;           // App Communication Route: 9/0
extern Applications_FullScreen* audSpecAnalyzer_App;        // App Communication Route: 9/1
extern Applications_StatusBar *spotify_App;                 // App Communication Route: 9/2
#endif