/**
 * @file mtb_github.h
 * @brief GitHub raw-content storage API for downloading and uploading assets to/from LittleFS and PSRAM.
 *
 * Provides functions to download PNG/SVG images directly into PSRAM buffers for immediate
 * rendering, download arbitrary GitHub repository files to LittleFS (SPIFFS), and manage
 * a deferred-download queue for assets requested while offline.
 *
 * Default repository is Meterbit/PXP_X1_STORAGE; alternative owners, repos, and PAT tokens
 * can be supplied per-call.
 */
#ifndef GITHUB_STORAGE
#define GITHUB_STORAGE
#include "Arduino.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "LittleFS.h"
#include "WiFiClient.h"

extern QueueHandle_t files2Download_Q; /**< Queue of File2Download_t items to fetch when internet becomes available. */

/**
 * @brief Storage action direction for GitHub upload/download operations.
 */
typedef enum {
    UPLOAD   = 1, /**< Transfer a local LittleFS file up to the GitHub repository. */
    DOWNLOAD      /**< Fetch a file from the GitHub repository to local LittleFS. */
} githubStrgAction_t;

/**
 * @brief Descriptor for a single GitHub upload or download operation.
 */
struct githubStrg_UpDwn_t {
    String              bucketFilePath; /**< Path of the file inside the GitHub repository. */
    String              flashFilePath;  /**< Destination / source path on LittleFS. */
    githubStrgAction_t  strgAction;     /**< Direction of transfer: UPLOAD or DOWNLOAD. */
};

/**
 * @brief Descriptor for a file queued for deferred download when offline.
 */
struct File2Download_t {
    char flashFilePath[50]  = {0}; /**< Target LittleFS path where the file will be saved. */
    char githubFilePath[50] = {0}; /**< Source path inside the GitHub repository. */
};

/**
 * @brief Download a PNG image from a URL directly into a PSRAM buffer.
 * @param url          Full HTTP/HTTPS URL of the PNG image.
 * @param outBuffer    Receives a pointer to the heap_caps-allocated PSRAM buffer; caller must free.
 * @param outSize      Receives the number of bytes stored in @p outBuffer.
 * @param outMimeType  Optional; receives the Content-Type reported by the server.
 * @return true  if the download succeeded and @p outBuffer is valid.
 * @return false on network error, allocation failure, or non-200 HTTP response.
 */
extern bool mtb_Download_Png_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);

/**
 * @brief Download an SVG image from a URL directly into a PSRAM buffer.
 * @param url          Full HTTP/HTTPS URL of the SVG file.
 * @param outBuffer    Receives a pointer to the heap_caps-allocated PSRAM buffer; caller must free.
 * @param outSize      Receives the number of bytes stored in @p outBuffer.
 * @param outMimeType  Optional; receives the Content-Type reported by the server.
 * @return true  if the download succeeded and @p outBuffer is valid.
 * @return false on network or allocation failure.
 */
extern bool mtb_Download_Svg_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType = nullptr);

/**
 * @brief Download an image from a URL and save it to LittleFS.
 * @param url           Full HTTP/HTTPS URL of the image.
 * @param pathInSPIFFS  Destination LittleFS path (e.g. "/icons/logo.png").
 * @return true  on success.
 * @return false on network error or filesystem write failure.
 * @note Check this function for possible bugs before use in production.
 */
extern bool mtb_Download_Online_Image_To_SPIFFS(const char* url, const char* pathInSPIFFS);

/**
 * @brief Create any missing parent directories on LittleFS for the given file path.
 * @param filePath  Full LittleFS file path whose parent directories should be created.
 * @return true  if all directories already exist or were created successfully.
 * @return false if a directory could not be created.
 */
extern bool mtb_Prepare_Flash_File_Path(const char* filePath);

/**
 * @brief Download a file from GitHub to LittleFS using a descriptor struct.
 * @param descriptor  Reference to a githubStrg_UpDwn_t specifying the paths and action.
 * @return true  on successful download.
 * @return false on HTTP or filesystem error.
 */
extern bool mtb_Download_Github_File_To_SPIFFS(githubStrg_UpDwn_t&);

/**
 * @brief Download a file from a GitHub repository to LittleFS by path strings.
 * @param bucketPath    Path of the file inside the GitHub repository.
 * @param flashPath     Destination LittleFS path.
 * @param account_Owner GitHub account owner (default "Meterbit").
 * @param repo_Name     Repository name (default "PXP_X1_STORAGE").
 * @param github_PAT    Personal Access Token for private repos (default "" = public).
 * @return true  on successful download.
 * @return false on HTTP or filesystem error.
 */
extern bool mtb_Download_Github_File_To_SPIFFS(const String& bucketPath, const String& flashPath, const String account_Owner = "Meterbit", const String repo_Name = "PXP_X1_STORAGE", const String github_PAT = "");

/**
 * @brief Download a file from a GitHub repository directly into a PSRAM buffer.
 * @param bucketPath    Path of the file inside the GitHub repository.
 * @param outBuffer     Receives a pointer to the heap_caps-allocated PSRAM buffer; caller must free.
 * @param outSize       Receives the number of bytes stored in @p outBuffer.
 * @param account_Owner GitHub account owner (default "Meterbit").
 * @param repo_Name     Repository name (default "PXP_X1_STORAGE").
 * @param github_PAT    Personal Access Token for private repos (default "" = public).
 * @return true  if the download succeeded and @p outBuffer is valid.
 * @return false on network or allocation failure.
 */
extern bool mtb_Download_Github_File_To_PSRAM(const String& bucketPath, uint8_t** outBuffer, size_t* outSize, const String account_Owner = "Meterbit", const String repo_Name = "PXP_X1_STORAGE", const String github_PAT = "");

#endif
