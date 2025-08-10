#ifndef MQTT_H
#define MQTT_H

#include "PicoMQTT.h"
#include "ArduinoJson.h"

extern JsonDocument dCommand;

struct mqtt_Data_Trans_t {
char topic_Listen[200];      // MQTT Topic coming from Client or sender or App
char topic_Response[200];    // MQTT Topic Response sent to Client or Sender or App.
void *payload; // MQTT message or payload
size_t pay_size;
};

class mtb_MQTT_Server : public PicoMQTT::Server {
protected:
    virtual void on_connected(const char *client_id) override;
    virtual void on_disconnected(const char *client_id) override;
    virtual void on_subscribe(const char *client_id, const char *topic) override;
    virtual void on_unsubscribe(const char *client_id, const char *topic) override;
};

    extern TaskHandle_t mtb_MQTT_Client_Task_Handle;
    extern TaskHandle_t mtb_MQTT_Server_Task_Handle;
    extern mtb_MQTT_Server mqttServer;
    extern PicoMQTT::Client mqttClient;

    // extern mqtt_Data_Trans_t config_Cmd_mqtt_data;
    // extern mqtt_Data_Trans_t apps_mqtt_data;
    // extern const char config_MQTT_Topic[];
    // extern const char apps_MQTT_Topic[];

    extern void initialize_MQTT();
    extern void start_MQTT_Server();
    extern void stop_MQTT_Server();
    extern void start_MQTT_Client();
    extern void stop_MQTT_Client();
    extern void MQTT_Server_Task(void *);
    extern void MQTT_Client_Task(void *);
#endif
