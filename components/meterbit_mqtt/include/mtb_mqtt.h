/**
 * @file mtb_mqtt.h
 * @brief Generic, multi-instance MQTT client library for PIXLPAL apps, built on PicoMQTT.
 *
 * Any number of Mtb_Mqtt_Client objects may be created and destroyed at runtime. Each one
 * owns its own broker connection, its own FreeRTOS task driving the PicoMQTT event loop, and
 * its own subscription set, so several clients (e.g. the PIXLPAL system client plus a
 * Home Assistant app client) run concurrently and independently.
 *
 * Typical app usage:
 * @code
 *   Mtb_Mqtt_Config_t cfg;
 *   cfg.host     = "homeassistant.local";
 *   cfg.port     = 1883;
 *   cfg.clientId = "pixlpal-m1";
 *   cfg.username = "mqtt_user";
 *   cfg.password = "mqtt_pass";
 *   cfg.willTopic   = "pixlpal/status";   // Home Assistant availability
 *   cfg.willPayload = "offline";
 *   cfg.willRetain  = true;
 *   cfg.taskName    = "HA Mqtt Sv";
 *
 *   Mtb_Mqtt_Client *ha = Mtb_Mqtt_Client::create(cfg);
 *   ha->onConnected([](Mtb_Mqtt_Client &c) {
 *       c.publish("pixlpal/status", "online", 0, true);
 *   });
 *   ha->subscribe("pixlpal/cmd/#", [](const char *topic, const char *payload, size_t len) {
 *       // handle command
 *   });
 *   ha->start();
 *   ...
 *   Mtb_Mqtt_Client::destroy(ha);   // stops the task and frees everything
 * @endcode
 *
 * @par Thread safety
 * Every client holds a recursive mutex that serialises its PicoMQTT event loop against
 * publish/subscribe calls made from other tasks, so app tasks may call publish() directly.
 * Callbacks (onConnected / message handlers) are invoked from the client's own task with
 * that mutex already held, so they may safely call publish() on the same client. They must
 * not block for long, because the connection is not serviced while a callback runs.
 */
#ifndef MTB_MQTT_H
#define MTB_MQTT_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ArduinoJson.h"
#include "PicoMQTT.h"
#include "psram_allocator.h"

class Mtb_Services; // from mtb_engine.h; kept opaque here to avoid a circular header include

/** @brief PSRAM-backed string used for all client configuration text. */
using Mtb_Mqtt_String = std::basic_string<char, std::char_traits<char>, PSRAMAllocator<char>>;

class Mtb_Mqtt_Client;

/**
 * @brief Callback invoked for a message arriving on a subscribed topic filter.
 * @param topic    Topic the message arrived on (never NULL).
 * @param payload  NUL-terminated payload buffer, valid only for the duration of the call.
 * @param len      Payload length in bytes, excluding the terminator.
 */
using Mtb_Mqtt_Msg_Cb = std::function<void(const char *topic, const char *payload, size_t len)>;

/** @brief Callback invoked for a connection lifecycle event on @p client. */
using Mtb_Mqtt_Event_Cb = std::function<void(Mtb_Mqtt_Client &client)>;

/**
 * @brief Everything needed to construct one MQTT client.
 *
 * Only @ref host is mandatory. The struct is copied into the client on create(), so it may
 * safely be a stack local — except for @ref caCertPem, which is stored by pointer.
 */
struct Mtb_Mqtt_Config_t {
    Mtb_Mqtt_String host;                /**< Broker hostname or IP. Required. */
    uint16_t        port = 1883;         /**< Broker TCP port. Use 8883 with @ref useTls. */
    Mtb_Mqtt_String clientId;            /**< MQTT client id. Empty lets the broker assign one. */
    Mtb_Mqtt_String username;            /**< Username, or empty for anonymous brokers. */
    Mtb_Mqtt_String password;            /**< Password, ignored when @ref username is empty. */

    /** @name Last Will and Testament
     *  Published by the broker if this client drops without a clean DISCONNECT. This is how
     *  Home Assistant availability (`online`/`offline`) is implemented.
     *  @{ */
    Mtb_Mqtt_String willTopic;           /**< Will topic. Empty disables the will. */
    Mtb_Mqtt_String willPayload;         /**< Will payload, e.g. "offline". */
    uint8_t         willQos = 0;         /**< Will QoS, 0 or 1. */
    bool            willRetain = false;  /**< Retain the will message. Usually true for availability. */
    /** @} */

    uint32_t reconnectIntervalMs = 5000;  /**< Delay between reconnect attempts. */
    uint32_t keepAliveMs         = 60000; /**< MQTT keep-alive; a PINGREQ is sent when idle this long. */
    uint32_t socketTimeoutMs     = 10000; /**< Socket read timeout while waiting for a broker reply. */

    /** @name TLS
     *  @{ */
    bool        useTls      = false;   /**< Connect over TLS. Raise @ref stackSize to >= 12288 when set. */
    const char *caCertPem   = nullptr; /**< CA certificate in PEM form. Must outlive the client. */
    bool        tlsInsecure = false;   /**< Skip certificate verification. Test brokers only. */
    /** @} */

    /** @name Client task
     *  @{ */
    Mtb_Mqtt_String taskName;          /**< FreeRTOS task name, truncated to 49 chars. */
    uint32_t        stackSize = 4096;  /**< Task stack in bytes. See @ref maxIncomingPayload. */
    uint8_t         priority  = 1;     /**< FreeRTOS task priority. */
    uint8_t         core      = 0;     /**< Core affinity, 0 or 1. */
    /** @} */

    /**
     * @brief Largest incoming payload delivered to a Mtb_Mqtt_Msg_Cb, in bytes.
     * @note PicoMQTT buffers the payload on the client task's stack, so this must stay well
     *       below @ref stackSize. Larger messages are dropped and counted in droppedMessages().
     */
    size_t maxIncomingPayload = 512;

    /** @brief Ticks the client task sleeps between event-loop iterations. */
    uint32_t loopDelayTicks = 1;
};

/**
 * @brief One independently running MQTT client connection.
 *
 * Created with create() and released with destroy(); the constructor is private so that every
 * instance is PSRAM-allocated and registered. A client is inert until start() is called, and
 * start()/stop() may be called repeatedly over its lifetime.
 */
class Mtb_Mqtt_Client {
public:
    /**
     * @brief Allocate and configure a client without connecting it.
     * @param config  Configuration; copied into the client.
     * @return New client, or nullptr if @p config.host is empty or allocation failed.
     *         Release it with destroy().
     */
    static Mtb_Mqtt_Client *create(const Mtb_Mqtt_Config_t &config);

    /**
     * @brief Stop and free a client created by create().
     * @param client  Client to destroy; set to nullptr on return. Passing nullptr is a no-op.
     * @note Blocks until the client's task has exited. Never call from that client's own
     *       task or from one of its callbacks.
     */
    static void destroy(Mtb_Mqtt_Client *&client);

    /**
     * @brief Launch the client task, which connects and then services the connection.
     * @return true if the task is running on return.
     * @note Connection happens asynchronously in the task; use onConnected() to be notified.
     */
    bool start();

    /**
     * @brief Ask the client task to disconnect and exit, then wait for it.
     * @param timeoutMs  Maximum time to wait for the task to unwind.
     * @return true if the task had exited by the time this returned.
     */
    bool stop(uint32_t timeoutMs = 5000);

    /** @brief True while this client's task is alive. */
    bool isRunning() const;

    /** @brief True while the TCP/MQTT session to the broker is established. */
    bool isConnected();

    /**
     * @brief Subscribe to a topic filter and route matching messages to @p callback.
     *
     * Subscriptions are remembered and automatically re-established after a reconnect.
     * Safe to call before start(), in which case the subscription is sent on first connect.
     *
     * @param topicFilter  MQTT filter; `+` and `#` wildcards are supported.
     * @param callback     Handler invoked from the client task.
     * @param qos          Requested QoS, 0 or 1.
     * @return true if the subscription was registered.
     * @note PicoMQTT re-subscribes at QoS 0 after a reconnect, so @p qos above 0 only applies
     *       to the current session. Treat incoming delivery as QoS 0 for design purposes.
     */
    bool subscribe(const char *topicFilter, Mtb_Mqtt_Msg_Cb callback, uint8_t qos = 0);

    /**
     * @brief Remove a subscription previously added with subscribe().
     * @param topicFilter  The exact filter string passed to subscribe().
     * @return true if the unsubscribe was issued.
     */
    bool unsubscribe(const char *topicFilter);

    /**
     * @brief Publish a raw payload.
     * @param topic    Destination topic.
     * @param payload  Payload bytes.
     * @param len      Payload length.
     * @param qos      0 or 1. QoS 2 is not supported and is sent as 1.
     * @param retain   Ask the broker to retain this as the topic's last known value.
     * @return true if the message was handed to the broker (and acknowledged, for QoS 1).
     */
    bool publish(const char *topic, const void *payload, size_t len, uint8_t qos = 0, bool retain = false);

    /** @brief Publish a NUL-terminated string payload. @see publish(const char*, const void*, size_t, uint8_t, bool) */
    bool publish(const char *topic, const char *payload, uint8_t qos = 0, bool retain = false);

    /**
     * @brief Serialise @p doc straight into the MQTT packet and publish it.
     *
     * The JSON is streamed, so large payloads (such as Home Assistant discovery configs)
     * never need a contiguous buffer.
     *
     * @param topic   Destination topic.
     * @param doc     Document to serialise.
     * @param qos     0 or 1.
     * @param retain  Retain flag; true for Home Assistant discovery and state topics.
     * @return true if the message was sent.
     */
    bool publishJson(const char *topic, const JsonDocument &doc, uint8_t qos = 0, bool retain = false);

    /** @brief Set the handler called from the client task once the broker session is up. */
    void onConnected(Mtb_Mqtt_Event_Cb callback);
    /** @brief Set the handler called from the client task when the session drops. */
    void onDisconnected(Mtb_Mqtt_Event_Cb callback);
    /** @brief Set the handler called from the client task when a connect attempt fails. */
    void onConnectFailed(Mtb_Mqtt_Event_Cb callback);

    /** @brief Task name this client runs under. */
    const char *name() const;
    /** @brief Read-only view of the configuration this client was created with. */
    const Mtb_Mqtt_Config_t &config() const { return _config; }
    /** @brief Number of incoming messages dropped for exceeding Mtb_Mqtt_Config_t::maxIncomingPayload. */
    uint32_t droppedMessages() const { return _droppedMessages; }

    /** @brief Arbitrary app-owned pointer; the library never touches it. */
    void *userData = nullptr;

    /**
     * @brief Take this client's lock, for direct use of raw().
     * @param timeoutMs  How long to wait for the lock.
     * @return true if the lock was acquired; call unlock() once for every successful lock().
     */
    bool lock(uint32_t timeoutMs = portMAX_DELAY);
    /** @brief Release a lock taken with lock(). */
    void unlock();

    /**
     * @brief The underlying PicoMQTT client, for functionality this wrapper does not expose.
     * @warning Only touch it between lock() and unlock(), or from inside a callback.
     */
    PicoMQTT::Client *raw() { return _pico; }

    /** @brief The service descriptor driving this client, for mtb_Launch/Kill_This_Service(). */
    Mtb_Services *service() const { return _service; }

    /** @brief Allocate the client in PSRAM. */
    void *operator new(std::size_t size);
    /** @brief Free a PSRAM-allocated client. */
    void operator delete(void *ptr);

private:
    explicit Mtb_Mqtt_Client(const Mtb_Mqtt_Config_t &config);
    ~Mtb_Mqtt_Client();

    Mtb_Mqtt_Client(const Mtb_Mqtt_Client &) = delete;
    Mtb_Mqtt_Client &operator=(const Mtb_Mqtt_Client &) = delete;

    static void taskEntry(void *service);
    void run(Mtb_Services *service);
    bool buildPico();

    Mtb_Mqtt_Config_t _config;
    PicoMQTT::Client *_pico   = nullptr;
    void             *_socket = nullptr; /**< Owned WiFiClientSecure when TLS is enabled. */
    Mtb_Services     *_service = nullptr;
    TaskHandle_t      _taskHandle = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    uint32_t          _droppedMessages = 0;

    Mtb_Mqtt_Event_Cb _onConnected;
    Mtb_Mqtt_Event_Cb _onDisconnected;
    Mtb_Mqtt_Event_Cb _onConnectFailed;

    Mtb_Mqtt_Client *_next = nullptr; /**< Intrusive link in the global client registry. */

    friend void mtb_Mqtt_Stop_All_Clients();
};

/** @brief Number of clients currently allocated via Mtb_Mqtt_Client::create(). */
size_t mtb_Mqtt_Client_Count();

/** @brief Stop every live client's task without destroying the clients. */
void mtb_Mqtt_Stop_All_Clients();

/**
 * @brief The PIXLPAL system MQTT client.
 *
 * Owns the status-bar MQTT icon, the internet-connected flag and the deferred GitHub asset
 * download trigger. Started and stopped by the Wi-Fi event handlers via mtb_Mqtt_Client_Sv;
 * apps should create their own clients rather than sharing this one.
 */
extern Mtb_Mqtt_Client *mtb_System_Mqtt_Client;

#endif // MTB_MQTT_H
