#include "ESPmDNS.h"
#include "mtb_wifi.h"
#include "freertos/timers.h"
#include "cJSON.h"
#include <esp_wifi.h>
#include "mtb_mqtt.h"
#include "mtb_ntp.h"
#include "mtb_nvs.h"
#include "mtb_graphics.h"
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "mtb_ble.h"
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"

static const char* TAG = "WIFI EVENTS";

struct Wifi_Credentials last_Successful_Wifi;

String ipStr;

//*************************************************************************************
// Function prototypes for specific event handling
void handle_wifi_connected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void handle_wifi_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void handle_ip_address_obtained(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
//**************************************************************************************

//**************************************************************************************
void wifi_CurrentContdNetwork(void){
    if (Mtb_Applications::pxpWifiConnectStatus == true){
    current_Network(WiFi.SSID().c_str(), ipStr.c_str());
    //ESP_LOGI(TAG, "WiFi.Status() is showing WL_CONNECTED.\n");
    }
    else{
    current_Network(NULL,NULL);
    //ESP_LOGI(TAG, "WiFi.Status() is not showing WL_CONNECTED. it sent NULL, NULL\n");
    }
}
//****************************************************************************************
// Handlers for the WiFi events
void handle_wifi_connected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGI(TAG, "PixlPal WiFi Connected to AP");
    //if(mtb_MQTT_Server_Task_Handle == NULL) start_MQTT_Server();
    if(mtb_MQTT_Client_Task_Handle == NULL) start_MQTT_Client();
    Mtb_Applications::pxpWifiConnectStatus = true;
}

void handle_ip_address_obtained(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    // ESP_LOGI(TAG, "Got IP address: %s ", ipStr.c_str());
    ipStr = WiFi.localIP().toString();
    current_Network(WiFi.SSID().c_str(), ipStr.c_str());
    mtb_Show_Status_Bar_Icon({"/batIcons/sta_mode.png", 1, 1});
    mtb_Write_Nvs_Struct("Wifi Cred", &last_Successful_Wifi, sizeof(Wifi_Credentials));
    mtb_Set_Status_RGB_LED(GREEN);
}

void handle_wifi_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (WiFi.status() == WL_CONNECT_FAILED) ESP_LOGI(TAG, "Heaven is in the heart"); //bleSettingsComSend("{\"pxp_command\": 1, \"connected\": 0}");
    ESP_LOGI(TAG, "PixlPal WiFi Disconnected from AP\n");
    if(mtb_MQTT_Client_Task_Handle != NULL) stop_MQTT_Client();
    Mtb_Applications::internetConnectStatus = false;
    if(Mtb_Applications::pxpWifiConnectStatus == true){
        mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", 1, 1});
        mtb_Show_Status_Bar_Icon({"/batIcons/wipe7x7.png", 10, 1});
        Mtb_Applications::pxpWifiConnectStatus = false;
    }
    mtb_Set_Status_RGB_LED(YELLOW);
}

//****************************************************************************************************
void mtb_Wifi_Init() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.setAutoReconnect(true); // Enable auto-reconnect
    // Registering handlers for specific WiFi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &handle_wifi_connected, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handle_ip_address_obtained, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handle_wifi_disconnected, NULL, NULL));
   
    mtb_Read_Nvs_Struct("Wifi Cred", &last_Successful_Wifi, sizeof(Wifi_Credentials));

    initialize_MQTT();
    WiFi.begin(last_Successful_Wifi.ssid, last_Successful_Wifi.pass);
    mtb_Start_This_Service(sntp_Time_Sv);
}
//**************************************************************************************