#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include "PicoMQTT.h"
#include "mtb_wifi.h"
#include "mtb_graphics.h"
#include "mtb_engine.h"
#include "mtb_mqtt.h"
#include "mtb_github.h"

static const char TAG[] = "MTB_MQTT";

EXT_RAM_BSS_ATTR TaskHandle_t mtb_MQTT_Client_Task_H = NULL;
// EXT_RAM_BSS_ATTR TaskHandle_t mtb_MQTT_Server_Task_H = NULL;

// EXT_RAM_BSS_ATTR QueueHandle_t config_Cmd_MQTT_queue = NULL;
// EXT_RAM_BSS_ATTR QueueHandle_t app_MQTT_queue = NULL; 

void mqtt_Server_Task(void*);
void mqtt_Client_Task(void*);
void mtb_Setup_Mqtt_Server(void);
void mtb_Setup_Mqtt_Client(void);

//const static int mqtt_queue_size = 4;
// mqtt_Data_Trans_t config_Cmd_mqtt_data;
// mqtt_Data_Trans_t apps_mqtt_data;

// static const char apps_MQTT_Topic[] = "Pixlpal/Apps";
// static const char config_MQTT_Topic[] = "Pixlpal/Config";
//mtb_MQTT_Server mqttServer;

PicoMQTT::Client mqttClient("broker.hivemq.com");
//PicoMQTT::Client mqttClient("test.mosquitto.org");


//EXT_RAM_BSS_ATTR Mtb_Services *mtb_Mqtt_Server_Sv = new Mtb_Services(mqtt_Server_Task, &mtb_MQTT_Server_Task_H, "MQTT Server Sv", 6144, 0, 1);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Mqtt_Client_Sv = new Mtb_Services(mqtt_Client_Task, &mtb_MQTT_Client_Task_H, "MQTT Client Sv", 4096, 0, 1);

// void mqtt_Server_Task(void* d_Service){
//   Mtb_Services *thisServ = (Mtb_Services *)d_Service;
//   mqttServer.begin();   // Start MQTT broker

//   mtb_Setup_Mqtt_Server();

//   while(MTB_SERV_IS_ACTIVE == pdTRUE) {
//     mqttServer.loop();
//     vTaskDelay(1);
//   }

//   //Stop MQTT Broker
//   mqttServer.stop();
//   mtb_Delete_This_Service(thisServ);
// }


void mqtt_Client_Task(void* d_Service){
    Mtb_Services *thisServ = (Mtb_Services *)d_Service;
    mqttClient.begin();   // Start MQTT Client
    mtb_Setup_Mqtt_Client();

  while(MTB_SERV_IS_ACTIVE == pdTRUE) {
    mqttClient.loop();
    vTaskDelay(1);
  }

    // Stop MQTT Client.
    mqttClient.stop();
    mtb_Delete_This_Service(thisServ);
}


// void mtb_MQTT_Server::on_connected(const char *client_id){
//   mtb_Show_Status_Bar_Icon({"/batIcons/phoneCont.png", 18, 1});
//   //mtb_Set_Status_RGB_LED(WHITE_SMOKE);
//   //Mtb_Applications::mqttPhoneConnectStatus = true;
// }

// void mtb_MQTT_Server::on_disconnected(const char *client_id){
//     mtb_Wipe_Status_Bar_Icon({"/batIcons/phoneCont.png", 18, 1});
//     //mtb_Set_Status_RGB_LED(GREEN);
//     //Mtb_Applications::mqttPhoneConnectStatus = false;
// }

// void mtb_MQTT_Server::on_subscribe(const char *client_id, const char *topic){
//   ESP_LOGI(TAG, "mqtt Client subscribed.\n");
// }

// void mtb_MQTT_Server::on_unsubscribe(const char *client_id, const char *topic){
//   ESP_LOGI(TAG, "mqtt Client un-subscribed.\n");
// }


// void mtb_Setup_Mqtt_Server(){
//   if(config_Cmd_MQTT_queue == NULL) config_Cmd_MQTT_queue = xQueueCreate(mqtt_queue_size,sizeof(mqtt_Data_Trans_t));     // REVISIT -> Put queue in PSRAM.
//   if(app_MQTT_queue == NULL) app_MQTT_queue = xQueueCreate(mqtt_queue_size,sizeof(mqtt_Data_Trans_t));     // REVISIT -> Put queue in PSRAM.

//   // Subscribe to a topic pattern and attach a callback
//   mqttServer.subscribe("#", [](const char *topic, const void *payload, size_t payload_size){
//       ESP_LOGI(TAG, "\n This Topic: %s came through. \n", topic);

//       if(strstr(topic, config_MQTT_Topic) != NULL){
//           if (payload_size){
//           strcpy(config_Cmd_mqtt_data.topic_Listen, topic);
//           strcpy(config_Cmd_mqtt_data.topic_Response, topic);
//           strcat(config_Cmd_mqtt_data.topic_Response, "/Response");

//           config_Cmd_mqtt_data.pay_size = payload_size;
//           config_Cmd_mqtt_data.payload = heap_caps_calloc(payload_size + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
//           memcpy(config_Cmd_mqtt_data.payload, payload, payload_size);
//           xQueueSend(config_Cmd_MQTT_queue, &config_Cmd_mqtt_data, portMAX_DELAY);
//           //mtb_Launch_This_Service(mqtt_Config_Parse_Sv);
//           }
//       }
//       else if(strstr(topic, apps_MQTT_Topic) != NULL){
//         if (payload_size){
//           strcpy(apps_mqtt_data.topic_Listen, topic);
//           strcpy(apps_mqtt_data.topic_Response, topic);
//           strcat(apps_mqtt_data.topic_Response, "/Response");

//           apps_mqtt_data.pay_size = payload_size;
//           apps_mqtt_data.payload = heap_caps_calloc(payload_size + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
//           memcpy(apps_mqtt_data.payload, payload, payload_size);
//           xQueueSend(app_MQTT_queue, &apps_mqtt_data, portMAX_DELAY);
//           //mtb_Launch_This_Service(appMQTT_Parser_Sv);
//         }
//       }
//       });
//     }

  void mtb_Setup_Mqtt_Client(){
    mqttClient.connected_callback = []{
      File2Download_t holderItem;
      mtb_Show_Status_Bar_Icon({"/batIcons/mqttCont2.png", 10, 1});
      if(xQueuePeek(files2Download_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) mtb_Launch_This_Service(mtb_GitHub_File_Dwnload_Sv);
      else Mtb_Applications::internetConnectStatus = true;
      mtb_Set_Status_RGB_LED(CYAN_PROCESS);
    };

    mqttClient.disconnected_callback = []{
      mtb_Wipe_Status_Bar_Icon({"/batIcons/mqttCont2.png", 10, 1});
      Mtb_Applications::internetConnectStatus = false;
      mtb_Set_Status_RGB_LED(WiFi.status() == WL_CONNECTED ? GREEN : BLACK);
    };
}