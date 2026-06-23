/**
 * @file mtb_ble.h
 * @brief BLE communication API for the PIXLPAL-M1 settings and application channels.
 *
 * Provides two logical BLE channels — a settings channel and an application channel —
 * each backed by a FreeRTOS queue and parser task. JSON commands received over BLE are
 * routed to handler functions (systemSettings, wifiSettings, etc.) via ArduinoJson
 * documents. Response messages are sent back to the paired device using route strings
 * defined as constants below.
 *
 * Initialise the subsystem with mtb_Ble_Comm_Init() after Wi-Fi is up.
 */
#ifndef MTB_BLE_CTRL
#define MTB_BLE_CTRL

#include "ArduinoJson.h"
#include "Arduino.h"

/** @defgroup mtb_ble_routes BLE Communication Routes
 *  @brief One-character string keys that identify the target handler for each BLE message.
 * @{ */
static const char mtb_App_Cycle_Settings_Route[] = "0"; /**< Route for app-cycle (carousel) configuration commands. */
static const char mtb_System_Settings_Route[]    = "1"; /**< Route for device system settings commands. */
static const char mtb_Wifi_Settings_Route[]      = "2"; /**< Route for Wi-Fi credential and network commands. */
static const char mtb_Ble_Settings_Route[]       = "3"; /**< Route for BLE name and pairing settings commands. */
static const char mtb_Software_Update_Route[]    = "4"; /**< Route for OTA firmware update commands and responses. */
/** @} */

extern JsonDocument dCommand; /**< Shared ArduinoJson document used to parse incoming BLE JSON payloads. */

/**
 * @brief Payload carrier for a single BLE message received on either channel.
 */
struct mtb_BleCom_Data_Trans_t {
    void   *payload;  /**< Pointer to the raw BLE message bytes. */
    size_t  pay_size; /**< Number of bytes in @p payload. */
};

extern QueueHandle_t setCom_queue;  /**< Queue for incoming settings-channel BLE messages. */
extern QueueHandle_t appCom_queue;  /**< Queue for incoming application-channel BLE messages. */

extern mtb_BleCom_Data_Trans_t setCom_data; /**< Staging struct for the settings-channel message being processed. */
extern mtb_BleCom_Data_Trans_t appCom_data; /**< Staging struct for the application-channel message being processed. */

extern TaskHandle_t ble_SetCom_Parser_Task_Handle; /**< Handle for the settings-channel BLE parser task. */
extern TaskHandle_t ble_AppCom_Parser_Task_Handle; /**< Handle for the application-channel BLE parser task. */

/**
 * @brief Send a JSON success/failure response to the companion app for the given command.
 * @param route     BLE route string identifying which channel to respond on.
 * @param appId     Application or command identifier echoed in the response.
 * @param response  pdPASS (default) for success, pdFAIL for failure.
 */
extern void mtb_Ble_App_Cmd_Respond_Success(const char* route, uint8_t appId, uint8_t response = pdPASS);

/**
 * @brief Initialise the BLE stack, create queues, and start the parser tasks.
 * @note  Must be called once from the system initialisation sequence.
 */
extern void mtb_Ble_Comm_Init(void);

/**
 * @brief Tear down the BLE stack and delete the parser tasks and queues.
 */
extern void mtb_Ble_Comm_Deinit(void);

/**
 * @brief Update the advertised BLE device name and restart advertising.
 * @param pxp_BLE_Name  Null-terminated string of the new BLE device name.
 */
extern void mtb_Ble_Change_Pxp_Ble_Name(const char* pxp_BLE_Name);

/**
 * @brief Extract an integer value at the given index from a comma-separated string.
 * @param data   Source string containing comma-separated integers.
 * @param index  Zero-based position of the integer to retrieve.
 * @return Parsed integer value, or -1 if the index is out of range.
 */
extern int getIntegerAtIndex(const String &data, int index);

/**
 * @brief Send a JSON message to the companion app on the settings BLE channel.
 * @param dSetRoute  Route string (one of the mtb_*_Route constants).
 * @param dMessage   JSON string payload to transmit.
 * @return 0 on success, non-zero on BLE transmission error.
 */
extern int bleSettingsComSend(const char* dSetRoute, const String& dMessage);

/**
 * @brief Send a JSON message to the companion app on the application BLE channel.
 * @param dAppID    Application identifier string used as the routing key.
 * @param dMessage  JSON string payload to transmit.
 * @return 0 on success, non-zero on BLE transmission error.
 */
extern int bleApplicationComSend(const char* dAppID, const String& dMessage);

/**
 * @brief FreeRTOS task entry point that processes incoming settings-channel BLE messages.
 * @param pvParam  Unused task parameter.
 */
extern void ble_SetCom_Parse_Task(void *pvParam);

/**
 * @brief FreeRTOS task entry point that processes incoming application-channel BLE messages.
 * @param pvParam  Unused task parameter.
 */
extern void ble_AppCom_Parse_Task(void *pvParam);

/* ---- Settings handler callbacks (called from ble_SetCom_Parse_Task) ---- */

/**
 * @brief Report the device's currently connected Wi-Fi network name and IP to the companion app.
 * @param ssid  SSID string of the connected network, or NULL if not connected.
 * @param ip    IP address string, or NULL if not connected.
 */
extern void current_Network(const char *ssid, const char *ip);

/**
 * @brief Handle a parsed system settings JSON command (brightness, volume, etc.).
 * @param doc  Reference to the parsed ArduinoJson document.
 */
extern void systemSettings(JsonDocument& doc);

/**
 * @brief Handle a parsed Wi-Fi credentials or network selection command.
 * @param doc  Reference to the parsed ArduinoJson document.
 */
extern void wifiSettings(JsonDocument& doc);

/**
 * @brief Handle a parsed BLE settings command (rename, pairing reset, etc.).
 * @param doc  Reference to the parsed ArduinoJson document.
 */
extern void bleSettings(JsonDocument& doc);

/**
 * @brief Handle a parsed OTA firmware update command.
 * @param doc  Reference to the parsed ArduinoJson document.
 */
extern void softwareUpdate(JsonDocument& doc);

/**
 * @brief Handle a parsed app-cycle (carousel order / enable) settings command.
 * @param doc  Reference to the parsed ArduinoJson document.
 */
extern void app_Cycle_Settings(JsonDocument& doc);

/* ---- Status notification helpers ---- */

/**
 * @brief Report the currently paired BLE device name to the companion app.
 * @param bleName  Null-terminated BLE device name string.
 */
void mtb_Current_Ble_Device(const char* bleName);

/**
 * @brief Apply a new BLE device name received from a settings command.
 * @param doc  Reference to the parsed ArduinoJson document containing the new name.
 */
void mtb_Set_Pxp_Ble_Name(JsonDocument& doc);

/**
 * @brief Report the current firmware version information to the companion app.
 * @param major      Major version string.
 * @param minor      Minor version string.
 * @param patch      Patch version string.
 * @param buildDate  Build date string.
 */
void mtb_Current_Software_Version(const char* major, const char* minor, const char* patch, const char* buildDate);

/**
 * @brief Immediately restart the device.
 */
void system_Restart_Device();

/**
 * @brief Initiate a graceful device shutdown.
 */
void system_Shutdown_Device();

#endif
