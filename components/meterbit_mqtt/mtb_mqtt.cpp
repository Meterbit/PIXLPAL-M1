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

EXT_RAM_BSS_ATTR TaskHandle_t mtb_MQTT_Client_Task_Handle = NULL;
// EXT_RAM_BSS_ATTR TaskHandle_t mtb_MQTT_Server_Task_Handle = NULL;
// EXT_RAM_BSS_ATTR QueueHandle_t config_Cmd_MQTT_queue = NULL;
// EXT_RAM_BSS_ATTR QueueHandle_t app_MQTT_queue = NULL; 
// StackType_t *mqttServerStack_Ptr;
// StaticTask_t *mqttServerTcb_psram_Ptr;
// StackType_t *mqttClientStack_Ptr;
// StaticTask_t *mqttClientTcb_psram_Ptr;

// const static int mqtt_queue_size = 4;

// mqtt_Data_Trans_t config_Cmd_mqtt_data;
// mqtt_Data_Trans_t apps_mqtt_data;
// const char apps_MQTT_Topic[] = "Pixlpal/Apps";
// const char config_MQTT_Topic[] = "Pixlpal/Config";
// mtb_MQTT_Server mqttServer;
PicoMQTT::Client mqttClient("broker.hivemq.com");
//PicoMQTT::Client mqttClient("test.mosquitto.org");

void mtb_MQTT_Server::on_connected(const char *client_id){
  showStatusBarIcon({"/batIcons/phoneCont.png", 18, 1});
  //set_Status_RGB_LED(WHITE_SMOKE);
  //Mtb_Applications::mqttPhoneConnectStatus = true;
}

void mtb_MQTT_Server::on_disconnected(const char *client_id){
    wipeStatusBarIcon({"/batIcons/phoneCont.png", 18, 1});
    //set_Status_RGB_LED(GREEN);
    //Mtb_Applications::mqttPhoneConnectStatus = false;
}

// void mtb_MQTT_Server::on_subscribe(const char *client_id, const char *topic){
//   ESP_LOGI(TAG, "client subscribed.\n");
// }

// void mtb_MQTT_Server::on_unsubscribe(const char *client_id, const char *topic){
//   ESP_LOGI(TAG, "client un-subscribed.\n");
// }

// void start_MQTT_Server(){
//   mqttServer.begin();   // Start MQTT broker
//   // mqttServerStack_Ptr = (StackType_t *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//   // mqttServerTcb_psram_Ptr = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
//   xTaskCreatePinnedToCore(MQTT_Server_Task, "MQTT_Se Task", 6144, NULL, 0, &mtb_MQTT_Server_Task_Handle, 1);   // DO NOT GIVE THESE TASK HIGH PRIORITY. Review Task Stack Size
//   //mtb_MQTT_Server_Task_Handle = xTaskCreateStaticPinnedToCore(MQTT_Server_Task, "MQTT_Server Task", 4096, NULL, 0, mqttServerStack_Ptr, mqttServerTcb_psram_Ptr, 1);
// }

void start_MQTT_Client(){
  mqttClient.begin();   // Start MQTT Client
  //mqttClientStack_Ptr = (StackType_t *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  //mqttClientTcb_psram_Ptr = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
  xTaskCreatePinnedToCore(MQTT_Client_Task, "MQTT_Cl Task", 6144, NULL, 0, &mtb_MQTT_Client_Task_Handle, 1);    // DO NOT GIVE THIS TASK A HIGH PRIORITY. Review Task Stack Size
  // mtb_MQTT_Server_Task_Handle = xTaskCreateStaticPinnedToCore(MQTT_Client_Task, "MQTT_Client Task", 4096, NULL, 0, mqttClientStack_Ptr, mqttClientTcb_psram_Ptr, 1);
}
// void stop_MQTT_Server(){
//   //Stop MQTT Broker
//   mqttServer.stop();
//   if(mtb_MQTT_Server_Task_Handle !=NULL) vTaskDelete(mtb_MQTT_Server_Task_Handle);
//   mtb_MQTT_Server_Task_Handle = NULL;
//   // heap_caps_free(mqttServerStack_Ptr);
//   // heap_caps_free(mqttServerTcb_psram_Ptr);
// }

void stop_MQTT_Client(){
  // Stop MQTT Client.
  if(mtb_MQTT_Client_Task_Handle !=NULL){
  mqttClient.stop();
  vTaskDelete(mtb_MQTT_Client_Task_Handle);
  mtb_MQTT_Client_Task_Handle = NULL;
  }
  // heap_caps_free(mqttClientStack_Ptr);
  // heap_caps_free(mqttClientTcb_psram_Ptr);
}

void initialize_MQTT(){

  // if(config_Cmd_MQTT_queue == NULL) config_Cmd_MQTT_queue = xQueueCreate(mqtt_queue_size,sizeof(mqtt_Data_Trans_t));     // A queue of character pointers
  // if(app_MQTT_queue == NULL) app_MQTT_queue = xQueueCreate(mqtt_queue_size,sizeof(mqtt_Data_Trans_t));     // A queue of character pointers

  // // Subscribe to a topic pattern and attach a callback
  // mqttServer.subscribe("#", [](const char *topic, const void *payload, size_t payload_size){
  //     ESP_LOGI(TAG, "\n This Topic: %s came through. \n", topic);

  //     // if(strstr(topic, config_MQTT_Topic) != NULL){
  //     //     if (payload_size){
  //     //     strcpy(config_Cmd_mqtt_data.topic_Listen, topic);
  //     //     strcpy(config_Cmd_mqtt_data.topic_Response, topic);
  //     //     strcat(config_Cmd_mqtt_data.topic_Response, "/Response");

  //     //     config_Cmd_mqtt_data.pay_size = payload_size;
  //     //     config_Cmd_mqtt_data.payload = heap_caps_calloc(payload_size + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
  //     //     memcpy(config_Cmd_mqtt_data.payload, payload, payload_size);
  //     //     xQueueSend(config_Cmd_MQTT_queue, &config_Cmd_mqtt_data, portMAX_DELAY);
  //     //     start_This_Service(mqtt_Config_Parse_Sv);
  //     //     }
  //     // }
  //     // else if(strstr(topic, apps_MQTT_Topic) != NULL){
  //     //   if (payload_size){
  //     //     strcpy(apps_mqtt_data.topic_Listen, topic);
  //     //     strcpy(apps_mqtt_data.topic_Response, topic);
  //     //     strcat(apps_mqtt_data.topic_Response, "/Response");

  //     //     apps_mqtt_data.pay_size = payload_size;
  //     //     apps_mqtt_data.payload = heap_caps_calloc(payload_size + 1, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
  //     //     memcpy(apps_mqtt_data.payload, payload, payload_size);
  //     //     xQueueSend(app_MQTT_queue, &apps_mqtt_data, portMAX_DELAY);
  //     //     start_This_Service(appMQTT_Parser_Sv);
  //     //   }
  //     //}
  //     });

  mqttClient.connected_callback = []{
    File2Download_t holderItem;
    showStatusBarIcon({"/batIcons/mqttCont2.png", 10, 1});
    if(xQueuePeek(files2Download_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE) start_This_Service(gitHubFileDwnload_Sv);
    else Mtb_Applications::internetConnectStatus = true;
    set_Status_RGB_LED(CYAN_PROCESS);
  };

  mqttClient.disconnected_callback = []{
    wipeStatusBarIcon({"/batIcons/mqttCont2.png", 10, 1});
    Mtb_Applications::internetConnectStatus = false;
    set_Status_RGB_LED(WiFi.status() == WL_CONNECTED ? GREEN : BLACK);
  };
  //IMPLEMENT ON_SUBSCRIBE, AND ON_UNSUBSCRIBE H
}

// void MQTT_Server_Task(void* arguments){
//   while(1) mqttServer.loop();
//   vTaskDelete(NULL);
// }

void MQTT_Client_Task(void* arguments){
  while(1) {
    mqttClient.loop();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}
