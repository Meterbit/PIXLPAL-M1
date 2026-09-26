# meterbit_mqtt

Generic, multi-instance MQTT client library for PIXLPAL apps, built on PicoMQTT.

Any number of `Mtb_Mqtt_Client` objects can be created and destroyed at runtime. Each one
owns its own broker connection, its own FreeRTOS task driving the PicoMQTT event loop, and
its own subscription set — so several clients run concurrently and independently. One app
talking to Home Assistant on the LAN and another talking to a public cloud broker do not
interfere with each other.

## Quick start

```cpp
#include "mtb_mqtt.h"

Mtb_Mqtt_Config_t cfg;
cfg.host     = "192.168.1.50";
cfg.port     = 1883;
cfg.clientId = "pixlpal-myapp";
cfg.taskName = "MyApp Mqtt Sv";

Mtb_Mqtt_Client *client = Mtb_Mqtt_Client::create(cfg);

client->onConnected([](Mtb_Mqtt_Client &c) {
    c.publish("pixlpal/hello", "up", 0, false);
});

client->subscribe("pixlpal/cmd/#", [](const char *topic, const char *payload, size_t len) {
    ESP_LOGI("MYAPP", "%s -> %s", topic, payload);
});

client->start();                    // launches this client's task

// ... later, when the app closes:
Mtb_Mqtt_Client::destroy(client);   // stops the task and frees everything; sets client = nullptr
```

## Lifecycle

| Call | Effect |
| --- | --- |
| `Mtb_Mqtt_Client::create(cfg)` | Allocates the client in PSRAM. Does **not** connect. |
| `subscribe()` / `onConnected()` | May be called before or after `start()`. |
| `start()` | Launches the client task, which connects and then services the connection. |
| `stop()` | Asks the task to disconnect and exit, then waits for it. |
| `Mtb_Mqtt_Client::destroy(c)` | `stop()` + free. Sets `c` to `nullptr`. |

`start()` / `stop()` may be called repeatedly over a client's lifetime. Reconnection after a
dropped link is automatic (`reconnectIntervalMs`), and stored subscriptions are re-issued on
every reconnect.

## Threading

Each client holds a **recursive mutex** that serialises its PicoMQTT event loop against
`publish()` / `subscribe()` calls from other tasks. App tasks may therefore call `publish()`
directly with no extra locking.

Callbacks run **on the client's own task with that mutex already held**, so they may safely
publish on the same client. They must not block for long — the connection is not serviced
while a callback runs. Push slow work onto a queue or another service instead.

For functionality this wrapper does not expose, use `raw()` between `lock()` and `unlock()`.

## Stack sizing

`maxIncomingPayload` bytes are buffered **on the client task's stack** when a message is
delivered. Keep `stackSize` comfortably above it:

| Scenario | `stackSize` | `maxIncomingPayload` |
| --- | --- | --- |
| Plain TCP, small payloads | 4096 (default) | 512 (default) |
| Plain TCP, larger JSON in | 8192 | 2048 |
| TLS (port 8883) | 12288–16384 | 512–2048 |

Messages larger than `maxIncomingPayload` are dropped and counted in `droppedMessages()`.

---

# Home Assistant

**This design works with Home Assistant's MQTT integration.** Everything HA requires is
supported by the underlying PicoMQTT client and exposed through `Mtb_Mqtt_Client`:

| Home Assistant requirement | Support |
| --- | --- |
| Mosquitto / HA broker on port 1883 | `host`, `port` |
| Username + password auth | `username`, `password` |
| Client ID | `clientId` |
| **Retained** publishes (discovery + state) | `retain` argument on every publish |
| QoS 0 and QoS 1 | `qos` argument |
| **LWT** for availability (`offline`) | `willTopic` / `willPayload` / `willRetain` |
| Wildcard subscriptions (`+`, `#`) | `subscribe()` |
| Auto-reconnect + auto-resubscribe | built in |
| JSON payloads | `publishJson()`, streamed straight into the packet |
| TLS on port 8883 | `useTls`, `caCertPem` |

HA speaks MQTT 3.1.1, which is exactly what PicoMQTT implements.

### Known limits (none of which block Home Assistant)

- **QoS 2 is not supported** — it is silently sent as QoS 1. HA does not require QoS 2.
- **Subscriptions are re-established at QoS 0 after a reconnect.** PicoMQTT hardcodes QoS 0
  when it replays stored subscriptions. Design command handling to tolerate QoS 0 delivery
  (HA's own default for command topics is QoS 0).
- **No MQTT 5** features (shared subscriptions, response topics). HA does not need them.
- Incoming payloads are capped by `maxIncomingPayload` — see *Stack sizing* above.

### MQTT discovery example

This registers a PIXLPAL display as a HA `light`, with availability, and handles commands:

```cpp
#include "mtb_mqtt.h"

static Mtb_Mqtt_Client *ha = nullptr;

static const char AVAILABILITY[] = "pixlpal/m1/status";
static const char STATE[]        = "pixlpal/m1/light/state";
static const char COMMAND[]      = "pixlpal/m1/light/set";
static const char DISCOVERY[]    = "homeassistant/light/pixlpal_m1/config";

void ha_App_Start() {
    Mtb_Mqtt_Config_t cfg;
    cfg.host     = "homeassistant.local";   // or the broker's IP
    cfg.port     = 1883;
    cfg.clientId = "pixlpal-m1";
    cfg.username = "mqtt_user";
    cfg.password = "mqtt_pass";

    // Availability: the broker publishes this if we drop off the network.
    cfg.willTopic   = AVAILABILITY;
    cfg.willPayload = "offline";
    cfg.willRetain  = true;
    cfg.willQos     = 1;

    cfg.taskName  = "HA Mqtt Sv";
    cfg.stackSize = 6144;

    ha = Mtb_Mqtt_Client::create(cfg);
    if (!ha) return;

    ha->onConnected([](Mtb_Mqtt_Client &c) {
        // 1. Announce ourselves to Home Assistant (retained, so HA finds us after a restart).
        JsonDocument discovery;
        discovery["name"]               = "PIXLPAL M1";
        discovery["unique_id"]          = "pixlpal_m1_light";
        discovery["schema"]             = "json";
        discovery["state_topic"]        = STATE;
        discovery["command_topic"]      = COMMAND;
        discovery["availability_topic"] = AVAILABILITY;
        discovery["brightness"]         = true;
        discovery["supported_color_modes"][0] = "rgb";

        JsonObject device = discovery["device"].to<JsonObject>();
        device["identifiers"][0] = "pixlpal_m1";
        device["name"]           = "PIXLPAL M1";
        device["manufacturer"]   = "Meterbit";
        device["model"]          = "M1";

        c.publishJson(DISCOVERY, discovery, 1, /* retain */ true);

        // 2. Mark ourselves online (retained, matching the will).
        c.publish(AVAILABILITY, "online", 1, /* retain */ true);

        // 3. Publish current state.
        JsonDocument state;
        state["state"]      = "ON";
        state["brightness"] = 200;
        c.publishJson(STATE, state, 0, true);
    });

    // Home Assistant sends "online" here after it restarts; re-announce so we are not
    // dropped from the device registry.
    ha->subscribe("homeassistant/status", [](const char *, const char *payload, size_t) {
        if (strcmp(payload, "online") == 0 && ha) {
            ha->publish(AVAILABILITY, "online", 1, true);
        }
    });

    ha->subscribe(COMMAND, [](const char *, const char *payload, size_t) {
        JsonDocument cmd;
        if (deserializeJson(cmd, payload) != DeserializationError::Ok) return;

        const bool on = strcmp(cmd["state"] | "OFF", "ON") == 0;
        mtb_Panel_Set_Brightness(on ? (cmd["brightness"] | 255) : 0);

        // Echo the new state back so HA's UI stays in sync.
        if (ha) ha->publishJson(STATE, cmd, 0, true);
    });

    ha->start();
}

void ha_App_Stop() {
    // Clean shutdown: say goodbye before the will would have fired.
    if (ha) ha->publish(AVAILABILITY, "offline", 1, true);
    Mtb_Mqtt_Client::destroy(ha);
}
```

With MQTT discovery enabled in HA (it is by default), the device appears automatically —
no `configuration.yaml` editing needed.

### TLS brokers (port 8883)

```cpp
static const char HA_CA_CERT[] = R"(-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----)";

cfg.port      = 8883;
cfg.useTls    = true;
cfg.caCertPem = HA_CA_CERT;   // must outlive the client
cfg.stackSize = 16384;        // TLS handshake needs the headroom
```

For a self-signed broker on a trusted LAN, `cfg.tlsInsecure = true` skips verification.

---

## The system client

`mtb_System_Mqtt_Client` is the PIXLPAL system client. It owns the status-bar MQTT icon, the
`internetConnectStatus` flag and the deferred GitHub asset download trigger, and it is
started/stopped by the Wi-Fi event handlers through `mtb_Mqtt_Client_Sv`.

**Apps should create their own clients rather than sharing this one** — a subscription or a
blocking callback added to the system client delays the status bar and asset downloads.
