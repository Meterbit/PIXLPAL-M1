/**
 * @file mtb_mqtt.cpp
 * @brief Multi-instance MQTT client implementation on top of PicoMQTT.
 *
 * Each Mtb_Mqtt_Client owns a PicoMQTT::Client, a socket, a recursive mutex and its own
 * Mtb_Services descriptor. The service descriptor is a private subclass carrying a back
 * pointer to the client, so the FreeRTOS task entry (which only receives the Mtb_Services*)
 * can recover the client without a global lookup table.
 *
 * This file also defines mtb_System_Mqtt_Client, the PIXLPAL-specific client that drives the
 * status bar icon, the internet-connected flag and the deferred GitHub download service.
 */
#include <atomic>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "PicoMQTT.h"
#include "mtb_engine.h"
#include "mtb_github.h"
#include "mtb_graphics.h"
#include "mtb_mqtt.h"
#include "mtb_wifi.h"

static const char TAG[] = "MTB_MQTT";

/** @brief Live client count, maintained by create()/destroy(). */
static std::atomic<size_t> s_clientCount{0};

/** @brief Head of the intrusive list of every live client. Guarded by s_registryMutex(). */
static Mtb_Mqtt_Client *s_registryHead = nullptr;

/**
 * @brief Lazily-created mutex guarding the client registry.
 *
 * Function-local so it is created on first use rather than at static-init time, which keeps
 * it safe regardless of the order global constructors run in.
 */
static SemaphoreHandle_t s_registryMutex() {
    static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    return mutex;
}

/**
 * @brief Mtb_Services descriptor extended with a back pointer to its owning client.
 *
 * Mtb_Services is a non-virtual, single-inheritance base, so the Mtb_Services* the engine
 * hands to the task entry can be static_cast back down to this type safely.
 */
class Mtb_Mqtt_Service_t : public Mtb_Services {
public:
    using Mtb_Services::Mtb_Services;
    Mtb_Mqtt_Client *owner = nullptr;
};

//****************************************************************************************
// Allocation

void *Mtb_Mqtt_Client::operator new(std::size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void Mtb_Mqtt_Client::operator delete(void *ptr) {
    heap_caps_free(ptr);
}

//****************************************************************************************
// Construction / destruction

Mtb_Mqtt_Client::Mtb_Mqtt_Client(const Mtb_Mqtt_Config_t &config) : _config(config) {
    if (_config.taskName.empty()) _config.taskName = "MQTT Client Sv";
    // Mtb_Services::serviceName is a fixed 50-byte buffer written with strcpy.
    if (_config.taskName.size() > 49) _config.taskName.resize(49);

    _mutex = xSemaphoreCreateRecursiveMutex();
}

Mtb_Mqtt_Client::~Mtb_Mqtt_Client() {
    if (!stop()) {
        // The task is still inside PicoMQTT and holds pointers into everything below.
        // Leaking a few kB is far better than freeing memory out from under a live task.
        ESP_LOGE(TAG, "%s: task did not exit; leaking client resources to avoid a use-after-free",
                 name());
        return;
    }

    if (_pico) {
        delete _pico;
        _pico = nullptr;
    }
    if (_socket) {
        delete static_cast<WiFiClientSecure *>(_socket);
        _socket = nullptr;
    }
    if (_service) {
        delete static_cast<Mtb_Mqtt_Service_t *>(_service);
        _service = nullptr;
    }
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

/**
 * @brief Build the PicoMQTT client and its socket from the stored configuration.
 * @return true on success.
 *
 * Deferred out of the constructor so a TLS socket is only allocated once the client is
 * actually started, and so a failed allocation can be reported to the caller.
 */
bool Mtb_Mqtt_Client::buildPico() {
    if (_pico) return true;

    if (_config.useTls) {
        WiFiClientSecure *secure = new WiFiClientSecure();
        if (!secure) {
            ESP_LOGE(TAG, "%s: WiFiClientSecure allocation failed", name());
            return false;
        }
        if (_config.caCertPem)      secure->setCACert(_config.caCertPem);
        else if (_config.tlsInsecure) secure->setInsecure();
        else ESP_LOGW(TAG, "%s: TLS requested with no CA certificate and tlsInsecure false; "
                           "the handshake will fail", name());
        _socket = secure;

        _pico = new PicoMQTT::Client(*secure,
                                     _config.host.c_str(), _config.port,
                                     _config.clientId.c_str(),
                                     _config.username.empty() ? nullptr : _config.username.c_str(),
                                     _config.password.empty() ? nullptr : _config.password.c_str(),
                                     _config.reconnectIntervalMs, _config.keepAliveMs,
                                     _config.socketTimeoutMs);
    } else {
        _pico = new PicoMQTT::Client(_config.host.c_str(), _config.port,
                                     _config.clientId.c_str(),
                                     _config.username.empty() ? nullptr : _config.username.c_str(),
                                     _config.password.empty() ? nullptr : _config.password.c_str(),
                                     _config.reconnectIntervalMs, _config.keepAliveMs,
                                     _config.socketTimeoutMs);
    }

    if (!_pico) {
        ESP_LOGE(TAG, "%s: PicoMQTT::Client allocation failed", name());
        return false;
    }

    // Last Will and Testament, sent as part of every CONNECT.
    _pico->will.topic   = _config.willTopic.c_str();
    _pico->will.payload = _config.willPayload.c_str();
    _pico->will.qos     = _config.willQos;
    _pico->will.retain  = _config.willRetain;

    _pico->connected_callback = [this] {
        ESP_LOGI(TAG, "%s: connected to %s:%u", name(), _config.host.c_str(), _config.port);
        if (_onConnected) _onConnected(*this);
    };
    _pico->disconnected_callback = [this] {
        ESP_LOGI(TAG, "%s: disconnected from %s", name(), _config.host.c_str());
        if (_onDisconnected) _onDisconnected(*this);
    };
    _pico->connection_failure_callback = [this] {
        ESP_LOGW(TAG, "%s: connection attempt to %s:%u failed", name(), _config.host.c_str(), _config.port);
        if (_onConnectFailed) _onConnectFailed(*this);
    };

    return true;
}

Mtb_Mqtt_Client *Mtb_Mqtt_Client::create(const Mtb_Mqtt_Config_t &config) {
    if (config.host.empty()) {
        ESP_LOGE(TAG, "create() refused: configuration has no broker host");
        return nullptr;
    }

    Mtb_Mqtt_Client *client = new Mtb_Mqtt_Client(config);
    if (!client) {
        ESP_LOGE(TAG, "create() failed: out of PSRAM");
        return nullptr;
    }
    if (!client->_mutex) {
        ESP_LOGE(TAG, "create() failed: could not create client mutex");
        delete client;
        return nullptr;
    }

    Mtb_Mqtt_Service_t *service = new Mtb_Mqtt_Service_t(
        &Mtb_Mqtt_Client::taskEntry, &client->_taskHandle, client->_config.taskName.c_str(),
        client->_config.stackSize, client->_config.priority, client->_config.core);
    if (!service) {
        ESP_LOGE(TAG, "create() failed: could not allocate service descriptor");
        delete client;
        return nullptr;
    }
    service->owner   = client;
    client->_service = service;

    if (xSemaphoreTake(s_registryMutex(), portMAX_DELAY) == pdTRUE) {
        client->_next  = s_registryHead;
        s_registryHead = client;
        xSemaphoreGive(s_registryMutex());
    }

    s_clientCount++;
    ESP_LOGI(TAG, "created client '%s' for %s:%u (%u live)",
             client->name(), config.host.c_str(), config.port, (unsigned) s_clientCount.load());
    return client;
}

void Mtb_Mqtt_Client::destroy(Mtb_Mqtt_Client *&client) {
    if (!client) return;
    ESP_LOGI(TAG, "destroying client '%s'", client->name());

    // Unlink before destruction so mtb_Mqtt_Stop_All_Clients() can never walk into a
    // client that is midway through being torn down.
    if (xSemaphoreTake(s_registryMutex(), portMAX_DELAY) == pdTRUE) {
        for (Mtb_Mqtt_Client **link = &s_registryHead; *link; link = &(*link)->_next) {
            if (*link == client) {
                *link = client->_next;
                break;
            }
        }
        client->_next = nullptr;
        xSemaphoreGive(s_registryMutex());
    }

    delete client;
    client = nullptr;
    if (s_clientCount) s_clientCount--;
}

//****************************************************************************************
// Task lifecycle

const char *Mtb_Mqtt_Client::name() const {
    return _config.taskName.c_str();
}

bool Mtb_Mqtt_Client::isRunning() const {
    return _taskHandle != nullptr;
}

/**
 * @brief FreeRTOS entry point shared by every client task.
 * @param service  The Mtb_Mqtt_Service_t the engine passed through, upcast to Mtb_Services*.
 */
void Mtb_Mqtt_Client::taskEntry(void *service) {
    Mtb_Services *thisServ = static_cast<Mtb_Services *>(service);
    Mtb_Mqtt_Client *client = static_cast<Mtb_Mqtt_Service_t *>(thisServ)->owner;
    client->run(thisServ);
}

/**
 * @brief Body of one client's task: connect, pump the event loop, then tear down.
 * @param service  This client's service descriptor, used for the run/stop flag.
 *
 * The mutex is taken and released around each loop() iteration rather than held for the
 * whole task, so publishes from app tasks only ever wait for a single iteration.
 */
void Mtb_Mqtt_Client::run(Mtb_Services *service) {
    Mtb_Services *thisServ = service;

    if (!buildPico()) {
        ESP_LOGE(TAG, "%s: setup failed, task exiting", name());
        mtb_Delete_This_Service(thisServ);
        return;
    }

    if (xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE) {
        _pico->begin();
        xSemaphoreGiveRecursive(_mutex);
    }

    while (MTB_SERV_IS_ACTIVE == pdTRUE) {
        if (xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE) {
            _pico->loop();
            xSemaphoreGiveRecursive(_mutex);
        }
        vTaskDelay(_config.loopDelayTicks ? _config.loopDelayTicks : 1);
    }

    if (xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE) {
        _pico->stop();
        xSemaphoreGiveRecursive(_mutex);
    }

    mtb_Delete_This_Service(thisServ); // clears _taskHandle and deletes this task
}

bool Mtb_Mqtt_Client::start() {
    if (!_service) return false;
    if (isRunning()) return true;

    mtb_Launch_This_Service(_service);
    return isRunning();
}

bool Mtb_Mqtt_Client::stop(uint32_t timeoutMs) {
    if (!_service) return true;
    if (!isRunning()) {
        _service->service_is_Running = pdFALSE;
        return true;
    }

    // Never wait on ourselves: the task cannot observe its own exit.
    if (xTaskGetCurrentTaskHandle() == _taskHandle) {
        mtb_Kill_This_Service(_service);
        return false;
    }

    mtb_Kill_This_Service(_service);

    const uint32_t pollMs = 10;
    uint32_t waitedMs = 0;
    while (_taskHandle != nullptr && waitedMs < timeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(pollMs));
        waitedMs += pollMs;
    }

    if (_taskHandle != nullptr) {
        ESP_LOGE(TAG, "%s: still running %ums after stop request", name(), (unsigned) timeoutMs);
        return false;
    }
    return true;
}

//****************************************************************************************
// Locking

bool Mtb_Mqtt_Client::lock(uint32_t timeoutMs) {
    if (!_mutex) return false;
    const TickType_t ticks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTakeRecursive(_mutex, ticks) == pdTRUE;
}

void Mtb_Mqtt_Client::unlock() {
    if (_mutex) xSemaphoreGiveRecursive(_mutex);
}

//****************************************************************************************
// Connection state, publish and subscribe

bool Mtb_Mqtt_Client::isConnected() {
    if (!_pico) return false;
    if (!lock(1000)) return false;
    const bool connected = _pico->connected();
    unlock();
    return connected;
}

bool Mtb_Mqtt_Client::subscribe(const char *topicFilter, Mtb_Mqtt_Msg_Cb callback, uint8_t qos) {
    if (!topicFilter || !callback) return false;

    // Subscribing before start() is allowed: PicoMQTT stores the filter and sends it on
    // connect, and re-sends every stored filter after a reconnect.
    if (!buildPico()) return false;
    if (!lock(5000)) return false;

    const size_t maxPayload = _config.maxIncomingPayload;
    std::function<void(char *, void *, size_t)> picoCb =
        [this, callback](char *topic, void *payload, size_t size) {
            callback(topic, static_cast<const char *>(payload), size);
        };

    _pico->subscribe(String(topicFilter), picoCb, maxPayload);

    // PicoMQTT::Client::subscribe() always requests QoS 0; re-issue at the requested QoS
    // for this session if the caller asked for QoS 1 and we are already connected.
    if (qos > 0 && _pico->connected()) {
        PicoMQTT::BasicClient *basic = _pico;
        basic->subscribe(String(topicFilter), qos > 1 ? 1 : qos);
    }

    unlock();
    return true;
}

bool Mtb_Mqtt_Client::unsubscribe(const char *topicFilter) {
    if (!topicFilter || !_pico) return false;
    if (!lock(5000)) return false;
    _pico->unsubscribe(String(topicFilter));
    unlock();
    return true;
}

bool Mtb_Mqtt_Client::publish(const char *topic, const void *payload, size_t len, uint8_t qos, bool retain) {
    if (!topic || !_pico) return false;
    if (!lock(5000)) return false;
    const bool sent = _pico->publish(topic, payload, len, qos, retain);
    unlock();
    return sent;
}

bool Mtb_Mqtt_Client::publish(const char *topic, const char *payload, uint8_t qos, bool retain) {
    return publish(topic, payload, payload ? strlen(payload) : 0, qos, retain);
}

bool Mtb_Mqtt_Client::publishJson(const char *topic, const JsonDocument &doc, uint8_t qos, bool retain) {
    if (!topic || !_pico) return false;
    if (!lock(5000)) return false;

    // Stream the JSON straight into the packet: OutgoingPacket is a Print, so no contiguous
    // buffer is needed even for large Home Assistant discovery payloads.
    auto packet = _pico->begin_publish(topic, measureJson(doc), qos, retain);
    serializeJson(doc, packet);
    const bool sent = packet.send();

    unlock();
    return sent;
}

//****************************************************************************************
// Event callbacks

void Mtb_Mqtt_Client::onConnected(Mtb_Mqtt_Event_Cb callback)    { _onConnected = callback; }
void Mtb_Mqtt_Client::onDisconnected(Mtb_Mqtt_Event_Cb callback) { _onDisconnected = callback; }
void Mtb_Mqtt_Client::onConnectFailed(Mtb_Mqtt_Event_Cb callback){ _onConnectFailed = callback; }

//****************************************************************************************
// Registry helpers

size_t mtb_Mqtt_Client_Count() {
    return s_clientCount;
}

//****************************************************************************************
// PIXLPAL system client
//
// Created at static-init time so that mtb_Mqtt_Client_Sv is a valid descriptor before the
// Wi-Fi event handlers can reach it. This mirrors how the other Mtb_Services singletons in
// the engine are constructed.

/** @brief Build the system client's configuration. */
static Mtb_Mqtt_Config_t mtb_System_Mqtt_Config() {
    Mtb_Mqtt_Config_t cfg;
    cfg.host      = "broker.hivemq.com";
    cfg.port      = 1883;
    cfg.taskName  = "MQTT Client Sv";
    cfg.stackSize = 4096;
    cfg.priority  = 0;
    cfg.core      = 1;
    return cfg;
}

EXT_RAM_BSS_ATTR Mtb_Mqtt_Client *mtb_System_Mqtt_Client = Mtb_Mqtt_Client::create(mtb_System_Mqtt_Config());

/** @brief Service descriptor the Wi-Fi handlers launch and kill. @see mtb_engine.h */
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Mqtt_Client_Sv =
    mtb_System_Mqtt_Client ? mtb_System_Mqtt_Client->service() : nullptr;

/**
 * @brief Attach the PIXLPAL status-bar, LED and GitHub-download behaviour to the system client.
 *
 * Runs once, at static-init time, via the initialiser of s_systemMqttWired below.
 */
static bool mtb_Wire_System_Mqtt_Client() {
    if (!mtb_System_Mqtt_Client) {
        ESP_LOGE(TAG, "system MQTT client could not be created");
        return false;
    }

    mtb_System_Mqtt_Client->onConnected([](Mtb_Mqtt_Client &) {
        File2Download_t holderItem;
        mtb_Show_Status_Bar_Icon({"/batIcons/mqttCont2.png", 10, 1});
        if (xQueuePeek(files2Download_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE)
            mtb_Launch_This_Service(mtb_GitHub_File_Dwnload_Sv);
        else
            Mtb_Applications::internetConnectStatus = true;
        mtb_Set_Status_RGB_LED(CYAN_PROCESS);
    });

    mtb_System_Mqtt_Client->onDisconnected([](Mtb_Mqtt_Client &) {
        mtb_Wipe_Status_Bar_Icon({"/batIcons/mqttCont2.png", 10, 1});
        Mtb_Applications::internetConnectStatus = false;
        mtb_Set_Status_RGB_LED(WiFi.status() == WL_CONNECTED ? GREEN : BLACK);
    });

    return true;
}

static const bool s_systemMqttWired = mtb_Wire_System_Mqtt_Client();

void mtb_Mqtt_Stop_All_Clients() {
    // Signal every client first, then wait, so the stop requests overlap instead of
    // serialising one full teardown timeout after another.
    if (xSemaphoreTake(s_registryMutex(), portMAX_DELAY) != pdTRUE) return;

    for (Mtb_Mqtt_Client *client = s_registryHead; client; client = client->_next) {
        if (client->_service) mtb_Kill_This_Service(client->_service);
    }
    for (Mtb_Mqtt_Client *client = s_registryHead; client; client = client->_next) {
        client->stop();
    }

    xSemaphoreGive(s_registryMutex());
}
