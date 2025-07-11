#include "mtbApps.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "polygonFX.h"

PolygonFX_t polygonFX = {
    "EUR/USD",
    "5VCg1MsJak0eubOx6jJmJVnlmkacVCWj",
    60
};

EXT_RAM_BSS_ATTR TaskHandle_t polygonFX_Task_H = NULL;
void polygonFX_App_Task(void *);

// BLE command functions
void setPolygonPair(DynamicJsonDocument &);
void setPolygonInterval(DynamicJsonDocument &);
void setPolygonAPIKey(DynamicJsonDocument &);

EXT_RAM_BSS_ATTR Applications_StatusBar *polygonFX_App = new Applications_StatusBar(polygonFX_App_Task, &polygonFX_Task_H, "Polygon FX", 8192);

void polygonFX_App_Task(void *dApplication){
    Applications *thisApp = (Applications *)dApplication;
    thisApp->app_EncoderFn_ptr = brightnessControl;
    ble_AppCom_Parser_Sv->register_BLE_Com_ServiceFns(setPolygonPair, setPolygonInterval, setPolygonAPIKey);
    appsInitialization(thisApp, statusBarClock_Sv);

    read_struct_from_nvs("polygonFX", &polygonFX, sizeof(PolygonFX_t));

    StaticJsonDocument<512> doc;
    char apiUrl[256];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    CentreText_t pairTxt(63, 16, Terminal6x8, YELLOW);
    CentreText_t priceTxt(63, 32, Terminal6x8, CYAN);

    while(THIS_APP_IS_ACTIVE == pdTRUE){
        while ((Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        const char *delim = "/";
        char from[8] = {0};
        char to[8] = {0};
        char *slash = strchr(polygonFX.pair, '/');
        if(slash){
            size_t len1 = slash - polygonFX.pair;
            strncpy(from, polygonFX.pair, len1);
            strncpy(to, slash + 1, sizeof(to)-1);
        }

        snprintf(apiUrl, sizeof(apiUrl),
                 "https://api.polygon.io/v3/reference/exchange-rates?from=%s&to=%s&apiKey=%s",
                 from, to, polygonFX.apiToken);

        printf("Requesting Polygon FX API: %s\n", apiUrl);

        http.begin(client, apiUrl);
        int httpCode = http.GET();
        if(httpCode == 200){
            String payload = http.getString();
            printf("HTTP GET response: %s\n", payload.c_str());

            DeserializationError err = deserializeJson(doc, payload);
            if(!err){
                float rate = doc["exchange_rate"] | 0.0;
                if(rate > 0.0){
                    pairTxt.writeString(String(polygonFX.pair));
                    priceTxt.writeString(String(rate, 4));
                }
            }
        } else {
            printf("HTTP GET failed: %d\n", httpCode);
        }
        http.end();

        int32_t waitMs = (polygonFX.updateInterval > 0 ? polygonFX.updateInterval * 1000 : 30000);
        for(int32_t t=0; t<waitMs && THIS_APP_IS_ACTIVE; t+=1000) delay(1000);
    }

    kill_This_App(thisApp);
}


void setPolygonPair(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *pair = dCommand["pair"];
    if(pair){
        strncpy(polygonFX.pair, pair, sizeof(polygonFX.pair)-1);
        write_struct_to_nvs("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    }
    ble_Application_Command_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

void setPolygonInterval(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    int16_t interval = dCommand["dInterval"];
    if(interval > 0){
        polygonFX.updateInterval = interval;
        write_struct_to_nvs("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    }
    ble_Application_Command_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

void setPolygonAPIKey(DynamicJsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String key = dCommand["api_key"];
    strncpy(polygonFX.apiToken, key.c_str(), sizeof(polygonFX.apiToken)-1);
    write_struct_to_nvs("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    ble_Application_Command_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

