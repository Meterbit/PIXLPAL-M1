#include "mtb_engine.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "polygonFX.h"

static const char TAG[] = "PXP_POLYGON_FX";

PolygonFX_t polygonFX = {
    "EUR/USD",
    "5VCg1MsJak0eubOx6jJmJVnlmkacVCWj",
    60
};

EXT_RAM_BSS_ATTR TaskHandle_t polygonFX_Task_H = NULL;
void polygonFX_App_Task(void *);

// BLE command functions
void setPolygonPair(JsonDocument &);
void setPolygonInterval(JsonDocument &);
void setPolygonAPIKey(JsonDocument &);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *polygonFX_App = new Mtb_Applications_StatusBar(polygonFX_App_Task, &polygonFX_Task_H, "Polygon FX", 8192);

void polygonFX_App_Task(void *dApplication){
    Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
    thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
    mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(setPolygonPair, setPolygonInterval, setPolygonAPIKey);
    mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);

    mtb_Read_Nvs_Struct("polygonFX", &polygonFX, sizeof(PolygonFX_t));

    StaticJsonDocument<512> doc;
    char apiUrl[256];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    Mtb_CentreText_t pairTxt(63, 16, Terminal6x8, YELLOW);
    Mtb_CentreText_t priceTxt(63, 32, Terminal6x8, CYAN);

    while(MTB_APP_IS_ACTIVE == pdTRUE){
        while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);

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

        ESP_LOGI(TAG, "Requesting Polygon FX API: %s\n", apiUrl);

        http.begin(client, apiUrl);
        int httpCode = http.GET();
        if(httpCode == 200){
            String payload = http.getString();
            ESP_LOGI(TAG, "HTTP GET response: %s\n", payload.c_str());

            DeserializationError err = deserializeJson(doc, payload);
            if(!err){
                float rate = doc["exchange_rate"] | 0.0;
                if(rate > 0.0){
                    pairTxt.mtb_Write_String(String(polygonFX.pair));
                    priceTxt.mtb_Write_String(String(rate, 4));
                }
            }
        } else {
            ESP_LOGI(TAG, "HTTP GET failed: %d\n", httpCode);
        }
        http.end();

        int32_t waitMs = (polygonFX.updateInterval > 0 ? polygonFX.updateInterval * 1000 : 30000);
        for(int32_t t=0; t<waitMs && MTB_APP_IS_ACTIVE; t+=1000) delay(1000);
    }

    mtb_End_This_App(thisApp);
}


void setPolygonPair(JsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    const char *pair = dCommand["pair"];
    if(pair){
        strncpy(polygonFX.pair, pair, sizeof(polygonFX.pair)-1);
        mtb_Write_Nvs_Struct("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    }
    mtb_Ble_App_Cmd_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

void setPolygonInterval(JsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    int16_t interval = dCommand["dInterval"];
    if(interval > 0){
        polygonFX.updateInterval = interval;
        mtb_Write_Nvs_Struct("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    }
    mtb_Ble_App_Cmd_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

void setPolygonAPIKey(JsonDocument &dCommand){
    uint8_t cmdNumber = dCommand["app_command"];
    String key = dCommand["api_key"];
    strncpy(polygonFX.apiToken, key.c_str(), sizeof(polygonFX.apiToken)-1);
    mtb_Write_Nvs_Struct("polygonFX", &polygonFX, sizeof(PolygonFX_t));
    mtb_Ble_App_Cmd_Respond_Success(polygonAppRoute, cmdNumber, pdPASS);
}

