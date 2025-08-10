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

extern bool downloadPNGImageToPSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);
extern bool downloadSVGImageToPSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);
//extern bool downloadOnlineImageToSPIFFS(const char* url, const char* pathInSPIFFS);
extern bool prepareFilePath(const char* filePath);
extern bool downloadGithubStrgFile(githubStrg_UpDwn_t&);
extern bool downloadGithubStrgFile(String bucketPath, String flashPath);
extern bool downloadGithubFileToPSRAM(const String& bucketPath, uint8_t** outBuffer, size_t* outSize);
#endif
