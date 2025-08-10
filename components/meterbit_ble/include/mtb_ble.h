#ifndef MTB_BLE_CTRL
#define MTB_BLE_CTRL

#include "ArduinoJson.h"
#include "Arduino.h"


// All Settings Communication Routes
static const char mtb_System_Settings_Route[] = "1";
static const char mtb_Wifi_Settings_Route[] = "2";
static const char mtb_Ble_Settings_Route[] = "3";
static const char mtb_Software_Update_Route[] = "4";

extern JsonDocument dCommand;

struct mtb_BleCom_Data_Trans_t {
void *payload; // BLE message or payload
size_t pay_size;
};

extern QueueHandle_t setCom_queue;
extern QueueHandle_t appCom_queue;

extern mtb_BleCom_Data_Trans_t setCom_data;
extern mtb_BleCom_Data_Trans_t appCom_data;

extern TaskHandle_t ble_SetCom_Parser_Task_Handle;
extern TaskHandle_t ble_AppCom_Parser_Task_Handle;

extern void mtb_Ble_App_Cmd_Respond_Success(const char*, uint8_t, uint8_t);
//extern void bleRestoreTimerCallBkFn(TimerHandle_t);

extern void mtb_Ble_Comm_Init(void);
extern void mtb_Ble_Comm_Deinit(void);

extern int getIntegerAtIndex(const String &data, int index);

extern int bleSettingsComSend(const char* dSetRoute, String dMessage);
extern int bleApplicationComSend(const char* dAppRoute, String dMessage);

extern void ble_SetCom_Parse_Task(void *);
extern void ble_AppCom_Parse_Task(void *);

extern void current_Network(const char *, const char *);
extern void systemSettings(JsonDocument&);
extern void wifiSettings(JsonDocument&);
extern void bleSettings(JsonDocument&);
extern void softwareUpdate(JsonDocument&);

//BLE SECTION
void mtb_Current_Ble_Device(const char*);
void mtb_Set_Pxp_Ble_Name(JsonDocument&);
void mtb_Current_Software_Version(const char*, const char*, const char*, const char*);
#endif