/**
 * @file mtb_mqtt.h
 * @brief Lightweight MQTT broker and client API built on PicoMQTT.
 *
 * Provides mtb_MQTT_Server, a subclass of PicoMQTT::Server with lifecycle callbacks
 * for connection, disconnection, and subscription events, plus a global mqttClient
 * instance for publishing and subscribing to topics on the local broker.
 */
#ifndef MQTT_H
#define MQTT_H

#include "PicoMQTT.h"

// #include "ArduinoJson.h"

// extern JsonDocument dCommand;

/**
 * @brief Payload carrier for MQTT messages exchanged between the device and a client app.
 */
struct mqtt_Data_Trans_t {
    char   topic_Listen[200];  /**< MQTT topic on which this message was received. */
    char   topic_Response[200]; /**< MQTT topic to which a response should be published. */
    void  *payload;            /**< Pointer to the raw message payload. */
    size_t pay_size;           /**< Size of @p payload in bytes. */
};

/**
 * @brief MQTT broker with overridable connection lifecycle hooks.
 *
 * Extends PicoMQTT::Server to log or react to client connect/disconnect and
 * topic subscribe/unsubscribe events. Start the broker by calling begin() and
 * call loop() from the main task.
 */
class mtb_MQTT_Server : public PicoMQTT::Server {
protected:
    /**
     * @brief Called when a client successfully connects to the broker.
     * @param client_id  MQTT client identifier string.
     */
    virtual void on_connected(const char *client_id) override;

    /**
     * @brief Called when a client disconnects from the broker.
     * @param client_id  MQTT client identifier string.
     */
    virtual void on_disconnected(const char *client_id) override;

    /**
     * @brief Called when a client subscribes to a topic.
     * @param client_id  MQTT client identifier string.
     * @param topic      Topic filter the client subscribed to.
     */
    virtual void on_subscribe(const char *client_id, const char *topic) override;

    /**
     * @brief Called when a client unsubscribes from a topic.
     * @param client_id  MQTT client identifier string.
     * @param topic      Topic filter the client unsubscribed from.
     */
    virtual void on_unsubscribe(const char *client_id, const char *topic) override;
};

    // extern mtb_MQTT_Server mqttServer;
    extern PicoMQTT::Client mqttClient; /**< Global MQTT client for publishing and subscribing to topics. */

    // extern mqtt_Data_Trans_t config_Cmd_mqtt_data;
    // extern mqtt_Data_Trans_t apps_mqtt_data;
    // extern const char config_MQTT_Topic[];
    // extern const char apps_MQTT_Topic[];

#endif
