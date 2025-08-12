#ifndef GITHUB_STORAGE
#define GITHUB_STORAGE
#include "Arduino.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "LittleFS.h"
#include "WiFiClient.h"

extern QueueHandle_t files2Download_Q;
typedef enum {UPLOAD = 1, DOWNLOAD} githubStrgAction_t;

struct githubStrg_UpDwn_t {
    String bucketFilePath;
    String flashFilePath;
    githubStrgAction_t strgAction;
};

    struct File2Download_t {
        char flashFilePath[50] = {0};
        char githubFilePath[50] = {0};
    };

extern bool mtb_Download_Png_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);
extern bool mtb_Download_Svg_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);
extern bool mtb_Download_Github_File_To_PSRAM(const String& bucketPath, uint8_t** outBuffer, size_t* outSize);
extern bool mtb_Download_Online_Image_To_SPIFFS(const char* url, const char* pathInSPIFFS); // Check this function for possible bugs


extern bool mtb_Prepare_Flash_File_Path(const char* filePath);
extern bool mtb_Download_Github_Strg_File(githubStrg_UpDwn_t&);
extern bool mtb_Download_Github_Strg_File(String bucketPath, String flashPath);

#endif
