#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "my_secret_keys.h"
#include "LittleFS.h"

static const char TAG[] = "AI_PIXEL_ART_APP";

#ifndef PIXELLAB_API_KEY
//#define PIXELLAB_API
static const char pixellab_token[] = "Insert your pixellab api key here"; // Get at https://pixellab.ai
#endif

#define PIXELLAB_API_URL    "https://api.pixellab.ai/v2/create-image-pixflux"
#define AI_ART_IMG_WIDTH    128
#define AI_ART_IMG_HEIGHT   64
#define AI_ART_HTTP_TIMEOUT_MS 45000
#define AI_ART_SPIFFS_PATH  "/ai_art.png"

struct AiArt_Data_t {
    char artPrompt[200];
};

struct AiArtRequest_t {
    char prompt[200];
};

EXT_RAM_BSS_ATTR AiArt_Data_t aiArtData;
EXT_RAM_BSS_ATTR TaskHandle_t aiPixelArt_Task_H = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t aiArtRequest_Q   = NULL;

void aiPixelArt_App_Task(void*);

// button callback
void aiArt_Button(button_event_t);

// BLE callbacks
void generateAiArt(JsonDocument&);
void setAiArtStyle(JsonDocument&);
void regenerateAiArt(JsonDocument&);

// API helper
static bool pixellab_Generate_To_PSRAM(const char* prompt, uint8_t** buffer, size_t* imageSize);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen* aiPixelArt_App;
Mtb_Applications* aiPixelArt_App_GetInstance() {
    if (!aiPixelArt_App)
        aiPixelArt_App = new Mtb_Applications_FullScreen(aiPixelArt_App_Task, &aiPixelArt_Task_H, "AI Pixel Art", {8,1}, 8192);
    return aiPixelArt_App;
}

MTB_REGISTER_APP(aiPixelArt_App, 8,1)

//************************************************************************************

void aiPixelArt_App_Task(void* dApplication) {
    Mtb_Applications* THIS_APP = (Mtb_Applications*)dApplication;
    THIS_APP->mtb_App_Set_EC11_Cb_Fns(aiArt_Button, mtb_Brightness_Control);
    THIS_APP->mtb_App_Set_Ble_Comm_Fns(generateAiArt, setAiArtStyle, regenerateAiArt);
    THIS_APP->mtb_App_Init();

    aiArtData = (AiArt_Data_t){""};
    mtb_Read_Nvs_Struct("aiArtData", &aiArtData, sizeof(AiArt_Data_t));

    if (aiArtRequest_Q == NULL)
        aiArtRequest_Q = xQueueCreate(3, sizeof(AiArtRequest_t));

    Mtb_CentreText_t titleText(0, 20, Terminal6x8, WHITE);
    Mtb_CentreText_t statusText(0, 32, Terminal6x8, CYAN);
    Mtb_CentreText_t promptText(0, 44, Terminal6x8, YELLOW);

    titleText.mtb_Write_Colored_String("AI PIXEL ART", WHITE);
    statusText.mtb_Write_Colored_String("Send a BLE prompt", CYAN);
    if (strlen(aiArtData.artPrompt) > 0)
        promptText.mtb_Write_Colored_String(aiArtData.artPrompt, YELLOW);

    if (litFS_Ready && LittleFS.exists(AI_ART_SPIFFS_PATH)) {
        Mtb_LocalImage_t savedImg;
        strlcpy(savedImg.imagePath, AI_ART_SPIFFS_PATH, sizeof(savedImg.imagePath));
        savedImg.xAxis = 0;
        savedImg.yAxis = 0;
        savedImg.scale = 1;
        mtb_Draw_Local_Png(savedImg);
    }

    uint8_t* buffer = nullptr;
    size_t imageSize = 0;
    AiArtRequest_t request;

    while (THIS_APP_IS_ACTIVE == pdTRUE) {

        while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE))
            delay(1000);

        if (xQueueReceive(aiArtRequest_Q, &request, pdMS_TO_TICKS(100)) != pdTRUE)
            continue;

        strlcpy(aiArtData.artPrompt, request.prompt, sizeof(aiArtData.artPrompt));
        mtb_Write_Nvs_Struct("aiArtData", &aiArtData, sizeof(AiArt_Data_t));

        mtb_Panel_Clear_Screen();
        statusText.mtb_Write_Colored_String("Generating...", ORANGE);

        if (pixellab_Generate_To_PSRAM(aiArtData.artPrompt, &buffer, &imageSize)) {
            bool saved = false;
            if (litFS_Ready && buffer && imageSize > 0) {
                File f = LittleFS.open(AI_ART_SPIFFS_PATH, "w", true);
                if (f) {
                    saved = (f.write(buffer, imageSize) == imageSize);
                    f.close();
                    if (!saved) LittleFS.remove(AI_ART_SPIFFS_PATH);
                }
            }
            if (saved) {
                heap_caps_free(buffer);
                buffer    = nullptr;
                imageSize = 0;
                Mtb_LocalImage_t img;
                strlcpy(img.imagePath, AI_ART_SPIFFS_PATH, sizeof(img.imagePath));
                img.xAxis = 0;
                img.yAxis = 0;
                img.scale = 1;
                mtb_Draw_Local_Png(img);
            } else {
                mtb_Draw_PSRAM_Png(&buffer, &imageSize);
            }
        } else {
            mtb_Panel_Clear_Screen();
            titleText.mtb_Write_Colored_String("AI PIXEL ART", WHITE);
            statusText.mtb_Write_Colored_String("Generation failed", RED);
            promptText.mtb_Write_Colored_String(aiArtData.artPrompt, YELLOW);
        }
    }

    if (aiArtRequest_Q) { vQueueDelete(aiArtRequest_Q); aiArtRequest_Q = NULL; }
    mtb_Delete_This_App(THIS_APP);
}

//************************************************************************************

void aiArt_Button(button_event_t button_Data) {
    switch (button_Data.type) {
    case BUTTON_CLICKED:
        switch (button_Data.count) {
        case 1:
            if (strlen(aiArtData.artPrompt) > 0) {
                AiArtRequest_t req;
                strlcpy(req.prompt, aiArtData.artPrompt, sizeof(req.prompt));
                if (aiArtRequest_Q) xQueueSend(aiArtRequest_Q, &req, 0);
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

//************************************************************************************

void generateAiArt(JsonDocument& dCommand) {
    const char* prompt = dCommand["prompt"];
    if (!prompt || strlen(prompt) == 0) return;
    AiArtRequest_t req;
    strlcpy(req.prompt, prompt, sizeof(req.prompt));
    if (aiArtRequest_Q) xQueueSend(aiArtRequest_Q, &req, 0);
}

void setAiArtStyle(JsonDocument& dCommand) {
    // Reserved for future style / palette options
}

void regenerateAiArt(JsonDocument& dCommand) {
    if (strlen(aiArtData.artPrompt) == 0) return;
    AiArtRequest_t req;
    strlcpy(req.prompt, aiArtData.artPrompt, sizeof(req.prompt));
    if (aiArtRequest_Q) xQueueSend(aiArtRequest_Q, &req, 0);
}

//************************************************************************************

static bool pixellab_Generate_To_PSRAM(const char* prompt, uint8_t** buffer, size_t* imageSize) {
    HTTPClient http;
    http.begin(PIXELLAB_API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + pixellab_token);
    http.setTimeout(AI_ART_HTTP_TIMEOUT_MS);

    // Build request body
    JsonDocument reqDoc;
    reqDoc["description"] = prompt;
    reqDoc["image_size"]["width"]  = AI_ART_IMG_WIDTH;
    reqDoc["image_size"]["height"] = AI_ART_IMG_HEIGHT;
    String reqBody;
    serializeJson(reqDoc, reqBody);

    int httpCode = http.POST(reqBody);
    if (httpCode != 200) {
        ESP_LOGE(TAG, "HTTP %d", httpCode);
        http.end();
        return false;
    }

    // Wait for the first body byte — read() is non-blocking and returns -1
    // immediately if the TCP buffer is empty, which ArduinoJson treats as EmptyInput.
    auto& stream = http.getStream();
    uint32_t t0 = millis();
    while (!stream.available() && millis() - t0 < 10000)
        delay(50);
    if (!stream.available()) {
        ESP_LOGE(TAG, "Response body timeout");
        http.end();
        return false;
    }

    // Filter parse: only keep image.base64 to minimise heap usage
    JsonDocument filter;
    filter["image"]["base64"] = true;

    JsonDocument respDoc;
    DeserializationError err = deserializeJson(respDoc, stream, DeserializationOption::Filter(filter));
    http.end();

    if (err != DeserializationError::Ok) {
        ESP_LOGE(TAG, "JSON parse: %s", err.c_str());
        printf("The request body is: \n\n %s \n", reqBody.c_str());
        return false;
    }

    const char* b64data = respDoc["image"]["base64"];
    if (!b64data) {
        ESP_LOGE(TAG, "No base64 field in response");
        return false;
    }

    // Strip "data:image/png;base64," prefix if present
    const char prefix[] = "data:image/png;base64,";
    if (strncmp(b64data, prefix, sizeof(prefix) - 1) == 0)
        b64data += sizeof(prefix) - 1;

    size_t b64Len     = strlen(b64data);
    size_t maxDecoded = (b64Len / 4) * 3 + 4;

    *buffer = (uint8_t*)heap_caps_malloc(maxDecoded, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!*buffer) {
        ESP_LOGE(TAG, "PSRAM alloc failed (%u bytes)", (unsigned)maxDecoded);
        return false;
    }

    size_t outLen = 0;
    int ret = mbedtls_base64_decode(*buffer, maxDecoded, &outLen,
                                    (const unsigned char*)b64data, b64Len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        heap_caps_free(*buffer);
        *buffer    = nullptr;
        *imageSize = 0;
        return false;
    }

    *imageSize = outLen;
    ESP_LOGI(TAG, "PNG ready: %u bytes", (unsigned)outLen);
    return true;
}
