#include <Arduino.h>
#include <HTTPClient.h>
//#include "mtb_github.h"
#include "mtb_text_scroll.h"
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "mtb_nvs.h"
#include "mtb_engine.h"
#include "mtb_pixel_art_png.h"
#include "psram_allocator.h"
#include "my_secret_keys.h"
#include "mtb_github.h"
#include "mbedtls/base64.h"
#include <vector>
#include <algorithm>



#ifndef PIXELLAB_API_KEY
//#define PIXELLAB_API
static const char pixellab_token[] = "Insert your pixellab api key here"; // Get at https://pixellab.ai
#endif

#define PIXELLAB_API_URL    "https://api.pixellab.ai/v2/create-image-pixflux"
#define AI_ART_IMG_WIDTH    128
#define AI_ART_IMG_HEIGHT   64
#define AI_ART_HTTP_TIMEOUT_MS 45000
#define AI_ART_SPIFFS_PATH  "/ai_art.png"


static const char TAG[] = "DIGITAL_ART_APP";

struct AiArtRequest_t {
    char prompt[200];
};

struct Digital_Data_t {
char displayArtName[200];
char artPrompt[200];
char apiToken[100] = {0};
uint8_t displayArtChangeIntv;
bool showPreloadedArt;
bool cycleAlldigitalArts;
};

EXT_RAM_BSS_ATTR Digital_Data_t digitalArtInfo;
EXT_RAM_BSS_ATTR TaskHandle_t digitalArt_Task_H = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t aiArtRequest_Q   = NULL;

void digitalArt_App_Task(void *);

// digital art helper functions
void getRamdomDigitalArtName(char* artName);
std::string constructPathFromName(const char* artName);

// button and encoder functions
void changeDigitalArtButton(button_event_t button_Data);

// bluetooth functions
void showPreloadedDigitalArt(JsonDocument&);

void generateAiArt(JsonDocument&);
void setAiArtStyle(JsonDocument&);
void setDigitalArtAPI_Key(JsonDocument&);

void next_DigitalArt(JsonDocument&);
void cycleAllDigitalArt(JsonDocument&);
void setDigitalChangeIntv(JsonDocument&);

// API helper
static bool pixellab_Generate_To_PSRAM(const char* prompt, uint8_t** buffer, size_t* imageSize);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *digitalArt_App;
Mtb_Applications* digitalArt_App_GetInstance() {
    if (!digitalArt_App) digitalArt_App = new Mtb_Applications_FullScreen(digitalArt_App_Task, &digitalArt_Task_H, "DigitalArt App", {8,1}, 8192);
    return digitalArt_App;
}

MTB_REGISTER_APP(digitalArt_App, 8, 1)

void digitalArt_App_Task(void* dApplication){
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(changeDigitalArtButton, mtb_Brightness_Control);
  THIS_APP->mtb_App_Set_Ble_Comm_Fns(showPreloadedDigitalArt, generateAiArt, setAiArtStyle, setDigitalArtAPI_Key, cycleAllDigitalArt, setDigitalChangeIntv, next_DigitalArt);
  THIS_APP->mtb_App_Init();
  //************************************************************************************ */

    // mtb_Draw_Local_Gif({"/mtblg/mtbStart.gif", 0, 0, 1});

    digitalArtInfo = (Digital_Data_t){
        "pixellab-African-neighborhood-street-wi-1778402642758.png",
        "A little girl with a blue hat and a yellow scarf, sitting on a green hill, looking at the sunset, in the style of a children's book illustration.",
        {0},
        100,
        true,
        true,
    };
    strcpy(digitalArtInfo.apiToken, pixellab_token);

    mtb_Read_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));

    if (aiArtRequest_Q == NULL)
        aiArtRequest_Q = xQueueCreate(3, sizeof(AiArtRequest_t));

    Mtb_CentreText_t titleText(0, 20, Terminal6x8, WHITE);
    Mtb_CentreText_t statusText(63, 20, Terminal8x12, CYAN);
    Mtb_CentreText_t promptText(0, 44, Terminal6x8, YELLOW);

    // titleText.mtb_Write_Colored_String("AI PIXEL ART", WHITE);
    // statusText.mtb_Write_Colored_String("Send a BLE prompt", CYAN);
    if (strlen(digitalArtInfo.artPrompt) > 0)
        // promptText.mtb_Write_Colored_String(digitalArtInfo.artPrompt, YELLOW);

    if (digitalArtInfo.showPreloadedArt != true && litFS_Ready && LittleFS.exists(AI_ART_SPIFFS_PATH)) {
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

    while (THIS_APP_IS_ACTIVE == pdTRUE){

        while ((Mtb_Applications::internetConnectStatus != true) && (THIS_APP_IS_ACTIVE == pdTRUE)) delay(1000);

        if(digitalArtInfo.showPreloadedArt){
                mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buffer, &imageSize, "Meterbit", "METERBIT_PRIV_RES", github_Token);
                mtb_Draw_PSRAM_Png(&buffer, &imageSize);

                while (THIS_APP_IS_ACTIVE == pdTRUE && Mtb_Applications::internetConnectStatus == true && digitalArtInfo.showPreloadedArt == true) {
                    if (digitalArtInfo.cycleAlldigitalArts == true) {
                        uint8_t changeArtIntv = digitalArtInfo.displayArtChangeIntv;
                        getRamdomDigitalArtName(digitalArtInfo.displayArtName);
                        mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buffer, &imageSize, "Meterbit", "METERBIT_PRIV_RES", github_Token);
                        ESP_LOGI(TAG, "Displaying \"%s\" for %d seconds", digitalArtInfo.displayArtName, changeArtIntv);
                        mtb_Draw_PSRAM_Png(&buffer, &imageSize);
                        while(changeArtIntv-->0 && THIS_APP_IS_ACTIVE == pdTRUE && digitalArtInfo.showPreloadedArt == true) delay(1000);
                    } else delay(1000);
                }
            } else {
            if (xQueueReceive(aiArtRequest_Q, &request, pdMS_TO_TICKS(100)) != pdTRUE) continue;

            strlcpy(digitalArtInfo.artPrompt, request.prompt, sizeof(digitalArtInfo.artPrompt));
            mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));

            mtb_Panel_Clear_Screen();
            mtb_Draw_Local_Gif({"/clkgif/clk31.gif", 47, 31, 0xFFFFFF});
            statusText.mtb_Write_Colored_String("Generating...", WHITE_SMOKE);

            if (pixellab_Generate_To_PSRAM(digitalArtInfo.artPrompt, &buffer, &imageSize)) {
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
                    mtb_Stop_Local_Gif(true);
                    mtb_Draw_Local_Png(img);
                } else {
                    mtb_Stop_Local_Gif(true);
                    mtb_Draw_PSRAM_Png(&buffer, &imageSize);
                }
            } else {
                mtb_Stop_Local_Gif(true);
                mtb_Panel_Clear_Screen();
                titleText.mtb_Write_Colored_String("AI PIXEL ART", WHITE);
                statusText.mtb_Write_Colored_String("Generation failed", RED);
                promptText.mtb_Write_Colored_String(digitalArtInfo.artPrompt, YELLOW);
            }
        }
    }

    if (aiArtRequest_Q) { vQueueDelete(aiArtRequest_Q); aiArtRequest_Q = NULL; }
    mtb_Delete_This_App(THIS_APP);
}

void changeDigitalArtButton(button_event_t button_Data){
    switch (button_Data.type){
    case BUTTON_RELEASED:
        break;
    case BUTTON_PRESSED:
        break;
    case BUTTON_PRESSED_LONG:
        digitalArtInfo.cycleAlldigitalArts = !digitalArtInfo.cycleAlldigitalArts;
        mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
        break;
    case BUTTON_CLICKED:
        switch (button_Data.count){
        case 1: {
            if (digitalArtInfo.showPreloadedArt){
                getRamdomDigitalArtName(digitalArtInfo.displayArtName);
                uint8_t* buf = nullptr;
                size_t sz = 0;
                mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buf, &sz, "Meterbit", "METERBIT_PRIV_RES", github_Token);
                mtb_Draw_PSRAM_Png(&buf, &sz);
            } else {
                if (strlen(digitalArtInfo.artPrompt) > 0) {
                    AiArtRequest_t req;
                    strlcpy(req.prompt, digitalArtInfo.artPrompt, sizeof(req.prompt));
                    if (aiArtRequest_Q) xQueueSend(aiArtRequest_Q, &req, 0);
                }
            }
            break;
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
}

//************************************************************************************ */
// bluetooth functions

void showPreloadedDigitalArt(JsonDocument& dCommand){
    digitalArtInfo.showPreloadedArt = dCommand["sceneMode"].as<bool>();
    mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

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

void setDigitalArtAPI_Key(JsonDocument& dCommand) {
    String userAPI_Key = dCommand["api_key"];
    strcpy(digitalArtInfo.apiToken, userAPI_Key.c_str());
    mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

void cycleAllDigitalArt(JsonDocument& dCommand){
    digitalArtInfo.cycleAlldigitalArts = dCommand["cycleArts"].as<bool>();
    digitalArtInfo.displayArtChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

void setDigitalChangeIntv(JsonDocument& dCommand){
    digitalArtInfo.displayArtChangeIntv = dCommand["dInterval"].as<uint8_t>();
    mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

void next_DigitalArt(JsonDocument& dCommand){
    const char* artName = dCommand["artName"];
    if (artName) strlcpy(digitalArtInfo.displayArtName, artName, 200);
    else getRamdomDigitalArtName(digitalArtInfo.displayArtName);
    ESP_LOGI(TAG, "Display Art: %s", digitalArtInfo.displayArtName);
    uint8_t* buf = nullptr;
    size_t sz = 0;
    mtb_Download_Github_File_To_PSRAM(constructPathFromName(digitalArtInfo.displayArtName).c_str(), &buf, &sz, "Meterbit", "METERBIT_PRIV_RES", github_Token);
    mtb_Draw_PSRAM_Png(&buf, &sz);
    mtb_Write_Nvs_Struct("DigitArtData", &digitalArtInfo, sizeof(Digital_Data_t));
}

// ************************************************************************************

// Shuffle-bag state: holds one full pass of indices in random order so that
// every entry is served exactly once before any entry repeats.
static std::vector<uint32_t> artShuffleBag;
static size_t artShuffleBagPos = 0;

static void reshuffleArtBag(uint32_t count) {
    artShuffleBag.resize(count);
    for (uint32_t i = 0; i < count; i++) artShuffleBag[i] = i;
    for (uint32_t i = count - 1; i > 0; i--) {
        uint32_t j = esp_random() % (i + 1);
        std::swap(artShuffleBag[i], artShuffleBag[j]);
    }
    artShuffleBagPos = 0;
}

void getRamdomDigitalArtName(char* artName){
    JsonDocument doc;
    deserializeJson(doc, json_Github_Pixel_Art_Pngs);
    JsonArray names = doc["names"].as<JsonArray>();
    uint32_t count = names.size();
    if (count == 0) return;

    if (artShuffleBag.size() != count || artShuffleBagPos >= artShuffleBag.size())
        reshuffleArtBag(count);

    uint32_t idx = artShuffleBag[artShuffleBagPos++];
    strlcpy(artName, names[idx].as<const char*>(), 200);
}

std::string constructPathFromName(const char* artName) {
    return std::string("Static_Pixel_Art/") + artName;
}

static bool pixellab_Generate_To_PSRAM(const char* prompt, uint8_t** buffer, size_t* imageSize) {
    HTTPClient http;
    http.begin(PIXELLAB_API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + digitalArtInfo.apiToken);
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
