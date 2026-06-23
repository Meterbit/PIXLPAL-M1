/**
 * @file mtb_engine.h
 * @brief PIXLPAL-M1 application and service lifecycle engine.
 *
 * Defines the two pillars of the firmware runtime model:
 *
 *   - **Mtb_Services**: lightweight FreeRTOS task wrappers that can be created,
 *     suspended, resumed, and deleted on demand. Services have no lifecycle beyond
 *     the task itself and never hold display ownership.
 *
 *   - **Mtb_Applications** (base) and its subclasses **Mtb_Applications_FullScreen**
 *     and **Mtb_Applications_StatusBar**: display-owning units that run in a FreeRTOS
 *     task, optionally suspend the previous app when launched, and can register BLE
 *     application-channel parser callbacks via mtb_App_Set_Ble_Comm_Fns().
 *
 * Apps and services are allocated in PSRAM via overloaded new/delete operators.
 * The engine also exposes the MTB_REGISTER_APP() macro for zero-cost self-registration,
 * utility string functions, the SpiRamJsonDocument typedef, and all system-wide
 * service/app extern handles.
 */
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
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_graphics.h"
#include <cstddef>
#include "esp_heap_caps.h"

/** @defgroup mtb_engine_constants Engine Constants
 * @{ */
#define APPS_PARSER_QUEUE_SIZE 5    /**< Depth of the BLE application-channel command queue. */
#define NVS_MEM_READ            0   /**< NVS access mode: read a stored struct. */
#define NVS_MEM_WRITE           1   /**< NVS access mode: write a struct to NVS. */
#define CYCLING_APP_SLOTS 5         /**< Maximum number of apps that can be queued in the carousel. */
/** @} */

/** @defgroup mtb_engine_task_macros Task Convenience Aliases
 * @brief Shorthand for the task-parameter pointer and the running-flag check inside task bodies.
 * @{ */
#define THIS_APP           thisApp                               /**< Local alias for the Mtb_Applications pointer received by a task. */
#define THIS_SERV          thisServ                              /**< Local alias for the Mtb_Services pointer received by a task. */
#define THIS_APP_IS_ACTIVE (thisApp->app_is_Running)             /**< pdTRUE while the application task should keep running. */
#define MTB_SERV_IS_ACTIVE (thisServ->service_is_Running)        /**< pdTRUE while the service task should keep running. */
/** @} */

/* ---- Global inter-task communication handles ---- */
extern QueueHandle_t clock_Update_Q;           /**< Queue used to push time-display refresh events to clock widgets. */
extern TaskHandle_t appLauncher_Task_H;        /**< Handle of the app-launcher task (owned by Mtb_App_Launcher_Sv). */
extern QueueHandle_t appLauncherQueue;         /**< Queue on which Mtb_Applications pointers are posted to trigger launch. */
extern QueueHandle_t nvsAccessQueue;           /**< Queue on which NvsAccessParams_t structs are posted for serialised NVS access. */
extern SemaphoreHandle_t nvsAccessComplete_Sem;/**< Binary semaphore given by nvsAccessTask when an NVS read/write completes. */

/**
 * @brief High-level action requested when a new app is launched or identified.
 */
enum Mtb_Action_On_App_t {
    LAUNCH_PXP_APP   = 1, /**< Start the target app as the active display owner. */
    KILL_PXP_APP,         /**< Terminate the target app and free its task. */
    SHOW_PXP_APP_UI,      /**< Show the target app's mobile companion UI without changing the running app. */
    IDENTIFY_PXP_APP,     /**< Respond with the target app's ID over BLE without launching it. */
};

/**
 * @brief Action to take on the currently running app when a new app is launched.
 */
enum Mtb_Do_Prev_App_t {
    SUSPEND_PREVIOUS_APP = 1, /**< Suspend (freeze) the current app so it can be resumed later. */
    DESTROY_PREVIOUS_APP,     /**< Delete the current app's task and free resources. */
    IGNORE_PREVIOUS_APP,      /**< Leave the current app running untouched. */
};

/**
 * @brief Status-bar overlay type shown while an app is active.
 */
enum Mtb_Status_Bar_t {
    NO_STATUS_BAR       = 0, /**< Full-screen mode — no status bar is drawn. */
    ICONS_ONLY_STATUS_BAR,   /**< Status bar shows battery/Wi-Fi/BLE icons only. */
    CLOCK_STATUS_BAR,        /**< Status bar shows a live hours:minutes clock. */
    WEEKDAY_STATUS_BAR,      /**< Status bar shows the current weekday name and date. */
};

/**
 * @brief Two-part application identity used by the engine and BLE command routing.
 */
struct Mtb_User_AppID_t {
    uint16_t GenApp; /**< Generic (category) application index. */
    uint16_t SpeApp; /**< Specific (sub-app) application index within the category. */
};

/**
 * @brief Carousel state: which apps are queued for automatic cycling and timing.
 */
struct Mtb_Apps_To_Cycle_t {
    bool              appsCycleActive;                             /**< true when automatic app cycling is enabled. */
    uint8_t           appsNoInCycle;                               /**< Number of apps currently in the cycle queue (max CYCLING_APP_SLOTS). */
    uint16_t          appCycleDuration;                            /**< Time each app is displayed in seconds. */
    Mtb_User_AppID_t  appsCycling[CYCLING_APP_SLOTS];             /**< Ordered list of app IDs to cycle through. */
    uint16_t          app_Frame_Buffers[CYCLING_APP_SLOTS][128][64]; /**< Cached last frame for each cycling app (128×64 pixels). */
};

/**
 * @brief Parameters posted to nvsAccessQueue for serialised NVS struct read/write.
 */
struct NvsAccessParams_t {
    bool        read_OR_Write; /**< NVS_MEM_READ or NVS_MEM_WRITE. */
    const char *key;           /**< NVS namespace key string. */
    void       *struct_ptr;    /**< Pointer to the struct to read into or write from. */
    size_t      struct_size;   /**< Size of the struct in bytes. */
};

extern Mtb_User_AppID_t     lastSavedApp;   /**< App ID loaded from NVS on boot; restores the last active app after a power cycle. */
extern Mtb_Apps_To_Cycle_t  cycling_Apps;   /**< Live carousel state; persisted to NVS by the BLE settings handler. */

/* ---- Serialised NVS task functions ---- */
extern void     appLauncherTask(void *);  /**< FreeRTOS task: dequeues Mtb_Applications* and calls appRunner(). */
extern void     appCyclingTask(void *);   /**< FreeRTOS task: cycles through cycling_Apps on a timed schedule. */
extern void     nvsAccessTask(void *);    /**< FreeRTOS task: serialises all NVS reads/writes to avoid concurrent flash access. */

/**
 * @brief Serialised NVS struct read — blocks until nvsAccessTask completes the operation.
 * @param key         NVS key string (max 15 chars).
 * @param struct_ptr  Destination buffer.
 * @param struct_size Byte count to read.
 * @return ESP_OK on success, an esp_err_t error code otherwise.
 */
extern esp_err_t mtb_Read_Nvs_Struct(const char* key, void* struct_ptr, size_t struct_size);

/**
 * @brief Serialised NVS struct write — blocks until nvsAccessTask completes the operation.
 * @param key         NVS key string (max 15 chars).
 * @param struct_ptr  Source buffer.
 * @param struct_size Byte count to write.
 * @return ESP_OK on success, an esp_err_t error code otherwise.
 */
extern esp_err_t mtb_Write_Nvs_Struct(const char *key, void *struct_ptr, size_t struct_size);

/* ---- Encoder / button callbacks ---- */
extern void (*mtb_Common_EncoderFn_ptr)(rotary_encoder_rotation_t); /**< Active encoder rotation callback; default is encoderDoNothing. */
extern void (*mtb_Common_ButtonFn_Ptr)(button_event_t);             /**< Active EC11 button callback; default is buttonDoNothing. */
extern void encoderDoNothing(rotary_encoder_rotation_t);            /**< No-op encoder callback used as a safe default. */
extern void buttonDoNothing(button_event_t);                        /**< No-op button callback used as a safe default. */
extern void mtb_Brightness_Control(rotary_encoder_rotation_t);      /**< Encoder callback: adjusts panel brightness. */
extern void mtb_Vol_Control_Encoder(rotary_encoder_rotation_t);     /**< Encoder callback: adjusts audio output volume. */
extern void randomButtonControl(button_event_t);                    /**< Button callback: triggers random audio pattern selection. */

/* ---- Utility functions ---- */

/**
 * @brief Convert a Unix timestamp to a human-readable date-time string.
 * @param unixTime             Seconds since the Unix epoch.
 * @param timezoneOffsetHours  UTC offset in hours (default 0 = UTC).
 * @return Formatted string, e.g. "2024-06-01 14:30:00".
 */
extern String mtb_Unix_Time_To_Readable(time_t unixTime, int timezoneOffsetHours = 0);

/**
 * @brief Format a large number with K/M/B suffix for compact display.
 * @param number  Value to format.
 * @return Formatted string, e.g. "1.23M".
 */
extern String mtb_Format_Large_Number(double number);

/**
 * @brief Percent-encode a string for use in URLs.
 * @param str  Input string to encode.
 * @return URL-encoded string.
 */
extern String mtb_Url_Encode(const char* str);

/**
 * @brief Extract and format the date portion of an ISO 8601 date-time string.
 * @param isoDateTime  Source string in "YYYY-MM-DDTHH:MM:SSZ" format.
 * @return Date string, e.g. "Jun 01, 2024".
 */
extern String mtb_Format_Iso_Date(const String& isoDateTime);

/**
 * @brief Extract and format the time portion of an ISO 8601 date-time string.
 * @param isoDateTime  Source string in "YYYY-MM-DDTHH:MM:SSZ" format.
 * @return Time string, e.g. "14:30".
 */
extern String mtb_Format_Iso_Time(const String& isoDateTime);

/**
 * @brief Format a Unix timestamp as a short date string.
 * @param timestamp  Seconds since the Unix epoch.
 * @return Date string, e.g. "01 Jun".
 */
extern String mtb_Format_Date_From_Timestamp(time_t timestamp);

/**
 * @brief Format a Unix timestamp as a short time string.
 * @param timestamp  Seconds since the Unix epoch.
 * @return Time string, e.g. "14:30".
 */
extern String mtb_Format_Time_From_Timestamp(time_t timestamp);

/**
 * @brief Return the current local time as an RFC 3339 string.
 * @return String in "YYYY-MM-DDTHH:MM:SS+00:00" format.
 */
extern String mtb_Get_Current_Time_RFC3339(void);

/**
 * @brief ArduinoJson allocator that places JSON variant pools in PSRAM.
 *
 * Pass this to BasicJsonDocument<SpiRamAllocator> (aliased as SpiRamJsonDocument)
 * when parsing large API responses that would overflow internal heap.
 */
struct SpiRamAllocator {
    /** @brief Allocate @p size bytes in PSRAM. */
    void* allocate(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    /** @brief Free a PSRAM allocation. */
    void deallocate(void* pointer) {
        heap_caps_free(pointer);
    }
    /** @brief Reallocate a PSRAM buffer to a new size. */
    void* reallocate(void* ptr, size_t new_size) {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
};

/** @brief ArduinoJson document type whose variant pool resides in PSRAM. */
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

typedef void (*encoderFn_ptr_t)(rotary_encoder_rotation_t);      /**< Encoder rotation callback function pointer type. */
typedef void (*mtb_Common_ButtonFn_Ptr_t)(button_event_t);       /**< Button event callback function pointer type. */

/**
 * @brief Lightweight FreeRTOS task wrapper for background services.
 *
 * A service is a periodic or event-driven task that does not own the display.
 * It is created with mtb_Launch_This_Service() and self-destructs by calling
 * mtb_Delete_This_Service(this) at the end of its task function. All Mtb_Services
 * instances are PSRAM-allocated via overloaded new/delete.
 */
class Mtb_Services {
public:
    void (*service)(void *) = nullptr;        /**< Task entry point; receives `this` as the parameter. */
    char     serviceName[50] = {0};           /**< Human-readable name passed to xTaskCreate. */
    uint32_t stackSize = 2048;                /**< FreeRTOS task stack size in bytes. */
    uint8_t  servicePriority = 1;             /**< FreeRTOS task priority. */
    TaskHandle_t* serviceT_Handle_ptr = NULL; /**< Pointer to the external TaskHandle_t variable for this service. */
    uint8_t  serviceCore = 0;                 /**< CPU core affinity (0 or 1). */
    uint8_t  service_is_Running = pdFALSE;    /**< pdTRUE while the service task is alive; checked via MTB_SERV_IS_ACTIVE. */

    /** @brief Allocate the service descriptor in PSRAM. */
    void *operator new(std::size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    /** @brief Free a PSRAM-allocated service descriptor. */
    void operator delete(void* ptr) { heap_caps_free(ptr); }

    Mtb_Services() {}; /**< Default constructor. */

    /**
     * @brief Construct and configure a service descriptor.
     * @param dService           Task entry function.
     * @param dServiceHandle_ptr Address of the caller's TaskHandle_t variable.
     * @param dServiceName       Display name for the task.
     * @param dStackSize         Stack size in bytes.
     * @param dServicePriority   FreeRTOS priority (default 1).
     * @param dServiceCore       CPU core affinity (default 0).
     */
    Mtb_Services(void (*dService)(void *), TaskHandle_t* dServiceHandle_ptr, const char* dServiceName, uint32_t dStackSize, uint8_t dServicePriority = 1, uint8_t dServiceCore = 0) {
        service = dService;
        serviceT_Handle_ptr = dServiceHandle_ptr;
        strcpy(serviceName, dServiceName);
        stackSize = dStackSize;
        servicePriority = dServicePriority;
        serviceCore = dServiceCore;
    }
};

/** @brief Function pointer type for BLE application-channel JSON command handlers. */
using bleCom_Parser_Fns_Ptr = void (*)(JsonDocument&);

/**
 * @brief Base class for all PIXLPAL-M1 display-owning applications.
 *
 * An application runs as a FreeRTOS task and exclusively owns the HUB75 display
 * while active. It registers encoder/button callbacks, optional BLE app-channel
 * handler arrays, and a mobile companion UI manifest. Applications are PSRAM-allocated.
 *
 * The engine manages the lifecycle: launch → (optionally suspend/destroy previous) →
 * run → (suspend or destroy on next launch). Use the subclasses
 * Mtb_Applications_FullScreen or Mtb_Applications_StatusBar rather than
 * instantiating this base class directly.
 */
class Mtb_Applications {
public:
    void (*application)(void *);   /**< Task entry point; receives `this` as the parameter. */
    char     appName[50] = {0};    /**< Human-readable task name. */
    uint32_t stackSize;            /**< FreeRTOS task stack size in bytes. */
    uint8_t  appPriority;          /**< FreeRTOS task priority. */
    TaskHandle_t* appHandle_ptr;   /**< Pointer to the external TaskHandle_t variable for this app. */
    uint8_t  appCore;              /**< CPU core affinity (0 or 1). */
    Mtb_User_AppID_t appID;        /**< Two-part app identity used by BLE routing and the carousel. */
    Mtb_Status_Bar_t appStatusBarType; /**< Status-bar overlay type displayed while this app is active. */

    const char* appMobile_Manifest; /**< JSON string describing the companion app's UI layout. */
    const char* appMobile_UiApi;    /**< JSON string describing the companion app's API endpoints. */
    const char* appMobile_Data;     /**< Optional JSON data payload sent to the companion app on launch. */

    Mtb_Services* appServices[10] = {nullptr}; /**< Array of up to 10 services started/stopped with this app. */
    void (*mtb_App_EncoderFn_ptr)(rotary_encoder_rotation_t) = encoderDoNothing; /**< Encoder callback active while this app runs. */
    void (*mtb_App_ButtonFn_Ptr)(button_event_t) = buttonDoNothing;               /**< Button callback active while this app runs. */
    void *parameters;                /**< Optional extra parameter pointer passed to the task. */
    bool fullScreen = false;         /**< true for full-screen apps; false for status-bar apps. */
    bool elementRefresh;             /**< Set true to trigger a redraw of on-screen widgets on the next loop tick. */
    uint8_t app_is_Running = pdFALSE;/**< pdTRUE while the task is alive; checked via THIS_APP_IS_ACTIVE. */

    Mtb_Do_Prev_App_t action_On_Prev_App = DESTROY_PREVIOUS_APP; /**< What to do to the previously running app on launch. */

    static Mtb_Applications *otaAppHolder;      /**< Pointer to the active OTA app instance (set during OTA to allow resume). */
    static Mtb_Applications *currentRunningApp; /**< Pointer to the currently display-owning app. */
    static Mtb_Applications *previousRunningApp;/**< Pointer to the app that was running before the current one. */

    /* ---- Device-wide status flags (shared across all apps) ---- */
    static bool     internetConnectStatus;       /**< true when MQTT broker is reachable (internet available). */
    static bool     usbPenDriveConnectStatus;    /**< true when a USB mass-storage device is physically connected. */
    static bool     usbPenDriveMounted;          /**< true when the USB mass-storage filesystem is mounted. */
    static bool     pxpWifiConnectStatus;        /**< true when the device is associated with a Wi-Fi AP. */
    static bool     bleAdvertisingStatus;        /**< true when BLE is advertising. */
    static bool     bleCentralContd;             /**< true when a BLE central (companion app) is connected. */
    static uint16_t bleCentralNegotiatedMtu;     /**< Negotiated BLE ATT MTU with the connected central; default 512. */
    static uint8_t  firmwareOTA_Status;          /**< Firmware OTA state: 6 = idle, other values indicate progress/error. */
    static uint8_t  spiffsOTA_Status;            /**< LittleFS/SPIFFS OTA state: 6 = idle. */
    static Mtb_Action_On_App_t actionOnApp;      /**< Pending action (launch, identify, show-UI) for the next app transition. */

    bleCom_Parser_Fns_Ptr bleAppComServiceFns[12] = {nullptr}; /**< Up to 12 BLE application-channel command handlers indexed by command number. */

    /**
     * @brief Create the FreeRTOS task for this application.
     * @return true if the task was successfully created.
     */
    bool appRunner();

    static void appResume(Mtb_Applications *);     /**< Resume a previously suspended app task. */
    static void appSuspend(Mtb_Applications *);    /**< Suspend the app task without deleting it. */
    static void appDestroy(Mtb_Applications *);    /**< Delete the app task and clear its handle. */

    /**
     * @brief Take the configured action on the previous app when a new one is launched.
     * @param action  SUSPEND_PREVIOUS_APP, DESTROY_PREVIOUS_APP, or IGNORE_PREVIOUS_APP.
     */
    static void actionOnPreviousApp(Mtb_Do_Prev_App_t action);

    /**
     * @brief Set the encoder and button callbacks that will be active while this app runs.
     * @param but_Fn  Button event handler (default buttonDoNothing).
     * @param enc_Fn  Encoder rotation handler (default mtb_Brightness_Control).
     */
    void mtb_App_Set_EC11_Cb_Fns(mtb_Common_ButtonFn_Ptr_t but_Fn = buttonDoNothing, encoderFn_ptr_t enc_Fn = mtb_Brightness_Control);

    /**
     * @brief Send the app's mobile companion UI manifest to the connected BLE central.
     */
    void mtb_App_Show_Mobile_UI(void);

    /**
     * @brief Store the companion app UI manifest and optional data payload.
     * @param manifest  JSON string describing the companion UI layout.
     * @param ui_api    JSON string describing the API endpoints.
     * @param data      Optional JSON data payload (default nullptr).
     */
    void mtb_App_Set_Mobile_UI(const char* manifest, const char* ui_api, const char* data = nullptr);

    /**
     * @brief Register up to 12 BLE application-channel command handler functions.
     * @param Fn_0  Handler for command index 0 (mandatory).
     * @param Fn_1 … Fn_11  Optional handlers for command indices 1–11.
     */
    void mtb_App_Set_Ble_Comm_Fns(bleCom_Parser_Fns_Ptr Fn_0, bleCom_Parser_Fns_Ptr Fn_1 = nullptr, bleCom_Parser_Fns_Ptr Fn_2 = nullptr, bleCom_Parser_Fns_Ptr Fn_3 = nullptr, bleCom_Parser_Fns_Ptr Fn_4 = nullptr, bleCom_Parser_Fns_Ptr Fn_5 = nullptr, bleCom_Parser_Fns_Ptr Fn_6 = nullptr, bleCom_Parser_Fns_Ptr Fn_7 = nullptr, bleCom_Parser_Fns_Ptr Fn_8 = nullptr, bleCom_Parser_Fns_Ptr Fn_9 = nullptr, bleCom_Parser_Fns_Ptr Fn_10 = nullptr, bleCom_Parser_Fns_Ptr Fn_11 = nullptr);

    /**
     * @brief Initialise the app and start up to 10 associated services.
     * @param pointer_0 … pointer_9  Optional service pointers to launch alongside the app.
     */
    void mtb_App_Init(Mtb_Services* pointer_0 = nullptr, Mtb_Services* pointer_1 = nullptr, Mtb_Services* pointer_2 = nullptr,
                      Mtb_Services* pointer_3 = nullptr, Mtb_Services* pointer_4 = nullptr, Mtb_Services* pointer_5 = nullptr,
                      Mtb_Services* pointer_6 = nullptr, Mtb_Services* pointer_7 = nullptr, Mtb_Services* pointer_8 = nullptr,
                      Mtb_Services* pointer_9 = nullptr);

    /** @brief Allocate the application descriptor in PSRAM. */
    void* operator new(std::size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    /** @brief Free a PSRAM-allocated application descriptor. */
    void operator delete(void* ptr) { heap_caps_free(ptr); }

    Mtb_Applications(); /**< Default constructor. */

    /**
     * @brief Construct and configure an application descriptor.
     * @param dApplication   Task entry function.
     * @param dAppHandle_ptr Address of the caller's TaskHandle_t variable.
     * @param dAppName       Display name for the task.
     * @param applicationID  Two-part app ID {GenApp, SpeApp}.
     * @param dStackSize     Stack size in bytes.
     * @param statusBarType  Status-bar overlay type for this app.
     */
    Mtb_Applications(void (*dApplication)(void *), TaskHandle_t* dAppHandle_ptr, const char* dAppName, Mtb_User_AppID_t applicationID, uint32_t dStackSize, Mtb_Status_Bar_t statusBarType);
};

/**
 * @brief Full-screen application subclass (no status bar, appCore=0 unless USB OTA).
 *
 * Use this for apps that paint the entire 128×64 display without a status bar.
 * Default app ID is {11,1}; default stack is 10 240 bytes.
 */
class Mtb_Applications_FullScreen : public Mtb_Applications {
public:
    Mtb_Applications_FullScreen();
    /**
     * @brief Construct a full-screen application descriptor.
     * @param dApplication   Task entry function.
     * @param dAppHandle_ptr Address of the caller's TaskHandle_t variable.
     * @param dAppName       Display name.
     * @param applicationID  App ID (default {11,1}).
     * @param dStackSize     Stack size in bytes (default 10240).
     */
    Mtb_Applications_FullScreen(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, Mtb_User_AppID_t applicationID = {11,1}, uint32_t dStackSize = 10240) :
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, applicationID, dStackSize, NO_STATUS_BAR) {
        fullScreen = true;
    }
};

/**
 * @brief Status-bar application subclass (draws a status bar above the content area).
 *
 * Use this for apps that co-exist with a clock, calendar, or icon strip at the top of the display.
 * Default status bar type is CLOCK_STATUS_BAR.
 */
class Mtb_Applications_StatusBar : public Mtb_Applications {
public:
    Mtb_Applications_StatusBar();
    /**
     * @brief Construct a status-bar application descriptor.
     * @param dApplication   Task entry function.
     * @param dAppHandle_ptr Address of the caller's TaskHandle_t variable.
     * @param dAppName       Display name.
     * @param applicationID  App ID (default {11,1}).
     * @param dStackSize     Stack size in bytes (default 10240).
     * @param statusBarType  Status-bar overlay (default CLOCK_STATUS_BAR).
     */
    Mtb_Applications_StatusBar(void (*dApplication)(void *), TaskHandle_t *dAppHandle_ptr, const char *dAppName, Mtb_User_AppID_t applicationID = {11,1}, uint32_t dStackSize = 10240, Mtb_Status_Bar_t statusBarType = CLOCK_STATUS_BAR) :
        Mtb_Applications(dApplication, dAppHandle_ptr, dAppName, applicationID, dStackSize, statusBarType) {
        fullScreen = false;
    }
};

/* ---- App and service lifecycle functions ---- */

/**
 * @brief Launch an application, optionally acting on the previously running app.
 * @param dApp        Pointer to the Mtb_Applications descriptor to launch.
 * @param actionOnApp What to do when this app is launched (default LAUNCH_PXP_APP).
 * @param do_Prv_App  What to do to the previous app (default DESTROY_PREVIOUS_APP).
 * @return Pointer to @p dApp (for chaining).
 */
extern Mtb_Applications* mtb_Launch_This_App(Mtb_Applications* dApp, Mtb_Action_On_App_t actionOnApp = LAUNCH_PXP_APP, Mtb_Do_Prev_App_t do_Prv_App = DESTROY_PREVIOUS_APP);

/**
 * @brief Start a service task if it is not already running.
 * @param pService  Pointer to the Mtb_Services descriptor to launch.
 */
extern void mtb_Launch_This_Service(Mtb_Services* pService);

/**
 * @brief Resume a previously suspended service task.
 * @param pService  Pointer to the Mtb_Services descriptor to resume.
 */
extern void mtb_Resume_This_Service(Mtb_Services* pService);

/**
 * @brief Suspend a running service task without deleting it.
 * @param pService  Pointer to the Mtb_Services descriptor to suspend.
 */
extern void mtb_Suspend_This_Service(Mtb_Services* pService);

/**
 * @brief Delete a service task and clear its running flag (called from within the task).
 * @param pService  Pointer to the Mtb_Services descriptor to delete.
 */
extern void mtb_Delete_This_Service(Mtb_Services *pService);

/**
 * @brief Forcibly terminate a service from outside its own task context.
 * @param pService  Pointer to the Mtb_Services descriptor to kill.
 */
extern void mtb_Kill_This_Service(Mtb_Services* pService);

/**
 * @brief Delete an application task and clear its running flag (called from within the task).
 * @param pApp  Pointer to the Mtb_Applications descriptor to delete.
 */
extern void mtb_Delete_This_App(Mtb_Applications *pApp);

/**
 * @brief Forcibly terminate an application from outside its own task context.
 * @param pApp  Pointer to the Mtb_Applications descriptor to kill.
 * @return Pointer to @p pApp (for chaining).
 */
extern Mtb_Applications* mtb_Kill_This_App(Mtb_Applications* pApp);

/**
 * @brief Find the registered application that matches the given ID and trigger an action.
 * @param id      Two-part app ID to look up in the registry.
 * @param action  Action to perform (LAUNCH, IDENTIFY, SHOW_UI).
 * @return Pointer to the matching Mtb_Applications descriptor, or nullptr if not found.
 */
extern Mtb_Applications* mtb_Identify_App_By_ID(Mtb_User_AppID_t id, Mtb_Action_On_App_t action);

/**
 * @brief Add an app to the carousel cycling list.
 * @param appID  Two-part ID of the app to add.
 * @return pdPASS if added, pdFAIL if the list is full or the ID is already present.
 */
extern uint8_t mtb_Add_App_To_Cycle_App_List(Mtb_User_AppID_t appID);

/**
 * @brief Remove an app from the carousel cycling list.
 * @param appID  Two-part ID of the app to remove.
 * @return pdPASS if removed, pdFAIL if the ID was not in the list.
 */
extern uint8_t mtb_Remove_App_From_Cycle_App_List(Mtb_User_AppID_t appID);

/* ---- Status-bar service task functions ---- */
extern TaskHandle_t statusBarClock_H;           /**< Task handle for the status-bar clock service. */
extern void mtb_StatusBar_Clock_Task(void*);    /**< FreeRTOS task: updates the HH:MM clock in the status bar every second. */
extern void mtb_StatusBar_Calendar_Task(void*); /**< FreeRTOS task: updates the weekday and date in the status bar when the day rolls over. */

/* ---- App self-registration structs and macro ---- */

/**
 * @brief Linked-list node used by the auto-registration system.
 *
 * Each registered app provides two function pointers: getRaw() returns the existing
 * PSRAM-allocated pointer; getInstance() allocates it on first call.
 */
struct Mtb_App_Registry_Entry {
    Mtb_User_AppID_t          appID;        /**< Identity of the registered app. */
    Mtb_Applications*         (*getRaw)();      /**< Returns the PSRAM pointer without allocating. */
    Mtb_Applications*         (*getInstance)(); /**< Allocates and returns the singleton on first call. */
    Mtb_App_Registry_Entry*   next;             /**< Next entry in the singly-linked registry list. */
};
extern Mtb_App_Registry_Entry* mtb_App_Registry_Head; /**< Head of the global app registry linked list (NULL-terminated). */

/**
 * @brief Static-init helper that prepends a registry entry to mtb_App_Registry_Head.
 *
 * Instantiate one of these (via MTB_REGISTER_APP) as a static variable in each app's
 * translation unit to register the app before main() runs.
 */
struct Mtb_App_AutoRegister {
    Mtb_App_AutoRegister(Mtb_App_Registry_Entry* entry) {
        entry->next = mtb_App_Registry_Head;
        mtb_App_Registry_Head = entry;
    }
};

/**
 * @brief Register an app with the engine at static-initialisation time.
 *
 * Place this macro at file scope in the app's `.cpp` file, after declaring the
 * `appName` pointer (Mtb_Applications*) and `appName##_GetInstance()` function.
 *
 * @param appName  The identifier of the Mtb_Applications* variable.
 * @param genApp   Generic (category) app index for the app ID.
 * @param speApp   Specific app index within the category.
 */
#define MTB_REGISTER_APP(appName, genApp, speApp)                             \
    static Mtb_Applications* appName##_getRaw()      { return appName; }      \
    static Mtb_App_Registry_Entry appName##_reg_entry = {                     \
        {genApp, speApp}, appName##_getRaw, appName##_GetInstance, nullptr    \
    };                                                                        \
    static Mtb_App_AutoRegister appName##_registrar(&appName##_reg_entry);

/* ---- System service extern handles ---- */
extern Mtb_Services* mtb_App_Launcher_Sv;          /**< Dequeues and launches app task requests. */
extern Mtb_Services* mtb_App_BleComm_Parser_Sv;    /**< Processes BLE application-channel JSON commands. */
extern Mtb_Services* mtb_Sett_BleComm_Parser_Sv;   /**< Processes BLE settings-channel JSON commands. */
extern Mtb_Services* mtb_Beep_Buzzer_Sv;           /**< Drives the piezo buzzer for beep patterns. */
extern Mtb_Services* mtb_Sntp_Time_Sv;             /**< Synchronises the system clock via SNTP. */
extern Mtb_Services* mtb_Read_Write_NVS_Sv;        /**< Serialises NVS struct read/write operations. */
extern Mtb_Services* mtb_Png_Local_ImageDrawer_Sv; /**< Renders PNG images from LittleFS to the panel. */
extern Mtb_Services* mtb_SvgLocal_ImageDrawer_Sv;  /**< Renders SVG images from LittleFS to the panel. */
extern Mtb_Services* mtb_Mqtt_Client_Sv;           /**< Runs the PicoMQTT client event loop. */
extern Mtb_Services *mtb_GitHub_File_Dwnload_Sv;   /**< Downloads deferred assets from GitHub to LittleFS. */
extern Mtb_Services* mtb_Audio_Out_Sv;             /**< I2S DAC audio output processing task. */
extern Mtb_Services* mtb_Audio_In_Sv;              /**< I2S microphone capture task. */
extern Mtb_Services* mtb_Usb_Audio_Sv;             /**< USB Audio Class (UAC) speaker task. */
extern Mtb_Services* mtb_Usb_Mass_Storage_Sv;      /**< USB mass-storage host task. */
extern Mtb_Services* mtb_Scroll_Tasks_Sv[];        /**< Array of text-scroll service instances. */
extern Mtb_Services* mtb_Encoder_Task_Sv;          /**< Reads EC11 encoder rotation events. */
extern Mtb_Services* mtb_Button_Task_Sv;           /**< Reads EC11 button press events. */

/* ---- Application-specific services ---- */
extern Mtb_Services* pixAnimClkGif_Sv;         /**< Pixel animation clock GIF renderer service. */
extern Mtb_Services* spotifyScreenUpdate_Sv;   /**< Spotify now-playing screen refresh service. */
extern Mtb_Services* mtb_Audio_Listening_Sv;   /**< Audio visualiser input-processing service. */

/* ---- Generic services shared by multiple apps ---- */
extern Mtb_Services* mtb_Status_Bar_Clock_Sv;    /**< Live HH:MM clock widget in the status bar. */
extern Mtb_Services* mtb_Status_Bar_Calendar_Sv; /**< Weekday + date widget in the status bar. */
extern Mtb_Services* mtb_App_Cycling_Sv;         /**< Carousel cycling service. */

/* ---- System applications (registered manually) ---- */
extern Mtb_Applications_FullScreen* usbOTA_Update_App;           /**< USB OTA firmware update app. App ID: {12,0}. */
extern Mtb_Applications_FullScreen* bleOTA_Update_App;           /**< BLE OTA firmware update app. App ID: {12,1}. */
extern Mtb_Applications_FullScreen* ghotaOTA_Check_Update_App;   /**< GitHub OTA update check app. App ID: {12,2}. */
extern Mtb_Applications_FullScreen* ghotaOTA_Perform_Update_App; /**< GitHub OTA firmware flash app. App ID: {12,3}. */

/* ---- Example applications ---- */
extern Mtb_Applications_FullScreen* exampleDesignMobileUI_App;   /**< Mobile UI design example app. App ID: {11,0}. */

#endif
