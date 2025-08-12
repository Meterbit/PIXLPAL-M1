
#include "mtb_github.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "mtb_ota.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"  // for PSRAM allocation

static const char TAG[] = "GITHUB_DOWNLOAD";

EXT_RAM_BSS_ATTR QueueHandle_t files2Download_Q = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t files2Download_Task_H = NULL;
void files2Download_Task(void*);

EXT_RAM_BSS_ATTR Mtb_Services *gitHubFileDwnload_Sv = new Mtb_Services(files2Download_Task, &files2Download_Task_H, "Github Dwnld", 10240, 2, pdFALSE, 1);

bool mtb_Download_Github_Strg_File(String bucketPath, String flashPath){
    // GitHub repository details
    const char* host = "api.github.com";
    const int httpsPort = 443;
    const char* owner = "Meterbit";
    const char* repo = "PXP_X1_STORAGE";
    const char* path = bucketPath.c_str();  // Path to the file in the repository
    const char* token = github_Token;  // GitHub personal access token
    

  WiFiClientSecure client;
  client.setInsecure();  // Disable certificate verification (not recommended for production)

  HTTPClient https;
  File file;

  // Build the request URL
  String url = String("https://api.github.com/repos/") + owner + "/" + repo + "/contents/" + path;

  //ESP_LOGI(TAG, "Requesting URL: %s \n", url.c_str());

  // Initialize the HTTP client
  if (!https.begin(client, url)) {
    return false;
  }

  // Set headers
  https.addHeader("User-Agent", "ESP32");
  https.addHeader("Accept", "application/vnd.github.v3.raw");
  https.addHeader("Authorization", "token " + String(token));

  // Send GET request
  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    // Open a file on LittleFS to save the downloaded content
    // ESP_LOGI(TAG, "The github file path is: %s \n", flashPath.c_str());

    if (mtb_Prepare_Flash_File_Path(flashPath.c_str())){
      file = LittleFS.open(flashPath, FILE_WRITE);
      if (!file)
      {
        ESP_LOGI(TAG, "Failed to open file for writing.\n");
        https.end();
        return false;
      }
    } else {
      ESP_LOGI(TAG, "File Path preparation failed.\n");
    }

    // Get the response stream
    WiFiClient * stream = https.getStreamPtr();

    // Read data from stream and write to file
    uint8_t buff[128] = { 0 };
    int len = https.getSize();
    //ESP_LOGI(TAG, "Filesize length: %d\n", len);

    while (https.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();

      if (size) {
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        file.write(buff, c);
        if (len > 0) {
          len -= c;
        }
      }
      delay(1);
    }

    file.close();
    //ESP_LOGI(TAG, "File downloaded and saved to LittleFS\n");
  } else {
    ESP_LOGI(TAG, "HTTP Error: %d\n", httpCode);
  }

  https.end();
  return true;
}

bool mtb_Download_Github_File_To_PSRAM(const String& bucketPath, uint8_t** outBuffer, size_t* outSize) {
    // GitHub repo details
    const char* host = "api.github.com";
    const int httpsPort = 443;
    const char* owner = "Meterbit";
    const char* repo = "PXP_X1_STORAGE";
    const char* path = bucketPath.c_str();
    const char* token = github_Token; // Personal Access Token

    WiFiClientSecure client;
    client.setInsecure();  // Insecure, for testing only

    HTTPClient https;

    String url = String("https://api.github.com/repos/") + owner + "/" + repo + "/contents/" + path;

    if (!https.begin(client, url)) {
        Serial.println("HTTPS begin failed.");
        return false;
    }

    https.addHeader("User-Agent", "ESP32");
    https.addHeader("Accept", "application/vnd.github.v3.raw");
    https.addHeader("Authorization", "token " + String(token));

    int httpCode = https.GET();

    if (httpCode != HTTP_CODE_OK) {
        ESP_LOGI(TAG, "HTTP GET failed: %d\n", httpCode);
        https.end();
        return false;
    }

    WiFiClient* stream = https.getStreamPtr();
    int contentLength = https.getSize();

    if (contentLength <= 0) {
        Serial.println("Invalid content length");
        https.end();
        return false;
    }

    // Allocate buffer in PSRAM
    uint8_t* buffer = (uint8_t*) heap_caps_malloc(contentLength, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        Serial.println("Failed to allocate PSRAM buffer");
        https.end();
        return false;
    }

    size_t bytesRead = 0;
    uint8_t temp[128];

    while (https.connected() && bytesRead < contentLength) {
        size_t available = stream->available();
        if (available) {
            int toRead = min((int)sizeof(temp), (int)(contentLength - bytesRead));
            int readLen = stream->readBytes(temp, toRead);
            memcpy(buffer + bytesRead, temp, readLen);
            bytesRead += readLen;
        }
        delay(1);
    }

    https.end();

    if (bytesRead != contentLength) {
        Serial.println("Mismatch in content size");
        free(buffer);
        return false;
    }

    *outBuffer = buffer;
    *outSize = bytesRead;
    return true;
}

bool mtb_Download_Github_Strg_File(githubStrg_UpDwn_t& downloadPaths){
    return mtb_Download_Github_Strg_File(downloadPaths.bucketFilePath, downloadPaths.flashFilePath);
}

void files2Download_Task(void* dService){
	Mtb_Services *thisService = (Mtb_Services*) dService;
  File2Download_t holderItem;
  bool dwnld_Succeed = false;

  while (xQueueReceive(files2Download_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE){
    statusBarNotif.mtb_Scroll_This_Text("UPDATING FILES", GREEN);
    dwnld_Succeed = mtb_Download_Github_Strg_File(String(holderItem.githubFilePath), String(holderItem.flashFilePath));
    if(dwnld_Succeed) statusBarNotif.mtb_Scroll_This_Text("FILE STORAGE UPDATE SUCCESSFUL", SANDY_BROWN);
  }

  Mtb_Applications::internetConnectStatus = true;
	mtb_End_This_Service(thisService);
}


bool mtb_Prepare_Flash_File_Path(const char* filePath) {
  String path(filePath);

  // If file already exists, no need to prepare directories
  if (LittleFS.exists(path)) {
    return true;
  }

  // Extract directory path
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash <= 0) {
    // File is at root, nothing to create
    return true;
  }

  String dirPath = path.substring(0, lastSlash);
  String subPath = "";

  // Recursively create each folder
  for (int i = 1; i < dirPath.length(); i++) {
    if (dirPath.charAt(i) == '/') {
      subPath = dirPath.substring(0, i);
      if (!LittleFS.exists(subPath)) {
        if (!LittleFS.mkdir(subPath)) {
          ESP_LOGI(TAG, "Failed to create directory: %s\n", subPath.c_str());
          return false;
        }
      }
    }
  }

  // Create final directory level if needed
  if (!LittleFS.exists(dirPath)) {
    if (!LittleFS.mkdir(dirPath)) {
      ESP_LOGI(TAG, "Failed to create directory: %s\n", dirPath.c_str());
      return false;
    }
  }

  return true;
}


bool mtb_Download_Online_Image_To_SPIFFS(const char* url, const char* pathInSPIFFS) {
  ESP_LOGI(TAG, "Downloading: %s\n", url);
  File file;
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    ESP_LOGI(TAG, "HTTP GET failed, error: %d\n", httpCode);
    http.end();
    return false;
  }

  // Check for valid content-type
  String contentType = http.header("Content-Type");
  ESP_LOGI(TAG, "Content-Type: %s\n", contentType.c_str());

  // Allowed MIME types (add more if needed)
  bool validContent =
    contentType.startsWith("image/") ||
    contentType == "application/octet-stream" ||
    contentType == "application/pdf";

  if (!validContent) {
    ESP_LOGI(TAG, "Blocked download: Unexpected content type: %s\n", contentType.c_str());
    http.end();
    return false;
  }

  if (mtb_Prepare_Flash_File_Path(pathInSPIFFS)) {
    file = LittleFS.open(pathInSPIFFS, FILE_WRITE);
    if (!file) {
      ESP_LOGI(TAG, "Failed to open file for writing\n");
      http.end();
      return false;
    }
  } else {
    ESP_LOGI(TAG, "File path preparation failed.\n");
    http.end();
    return false;
  }

  int total = http.writeToStream(&file);

  file.close();
  http.end();

  ESP_LOGI(TAG, "File saved to %s (%d bytes)\n", pathInSPIFFS, total);
  return (total > 0);
}


// Download image to PSRAM or heap with auto fallback, timeout, type detection, and support for chunked transfer
bool mtb_Download_Png_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType) {
  //ESP_LOGI(TAG, "[Download] Starting: %s\n", url);

  HTTPClient http;
  http.setTimeout(15000); // 15-second timeout
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    //ESP_LOGI(TAG, "[Download] HTTP GET failed: %d\n", httpCode);
    http.end();
    return false;
  }

  String contentType = http.header("Content-Type");
  if (outMimeType) *outMimeType = contentType;
  //ESP_LOGI(TAG, "[Download] Content-Type: %s\n", contentType.c_str());

if (contentType.length() == 0) {
  //ESP_LOGI(TAG, "[Download] Warning: No Content-Type provided. Proceeding anyway...\n");
} else if (!contentType.startsWith("image/") && contentType != "application/octet-stream") {
  //ESP_LOGI(TAG, "[Download] Unsupported MIME type: %s. Aborting.\n", contentType.c_str());
  http.end();
  return false;
}

  int contentLen = http.getSize();
  bool isChunked = (contentLen <= 0);
  if (isChunked) ESP_LOGI(TAG, "[Download] Chunked transfer detected.\n");

  const size_t bufferCapacity = isChunked ? 256 * 1024 : contentLen; // Allocate 256KB for chunked by default
  uint8_t* buffer = (uint8_t*)ps_malloc(bufferCapacity);
  bool usedHeap = false;

  if (!buffer) {
    ESP_LOGI(TAG, "[Download] PSRAM allocation failed. Trying heap...\n");
    buffer = (uint8_t*)malloc(bufferCapacity);
    usedHeap = true;
    if (!buffer) {
      ESP_LOGI(TAG, "[Download] Memory allocation failed.\n");
      http.end();
      return false;
    }
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t totalRead = 0;
  uint32_t lastReadTime = millis();

  while (http.connected()) {
    size_t available = stream->available();
    if (available) {
      size_t toRead = min(available, bufferCapacity - totalRead);
      int bytesRead = stream->readBytes(buffer + totalRead, toRead);
      if (bytesRead <= 0) break;
      totalRead += bytesRead;
      lastReadTime = millis();

      if (!isChunked && totalRead >= contentLen) break;
      if (isChunked && totalRead >= bufferCapacity - 512) break; // Prevent overflow
    } else {
      if (millis() - lastReadTime > 10000) { // 10s timeout
        ESP_LOGI(TAG, "[Download] Stream timeout.\n");
        free(buffer);
        http.end();
        return false;
      }
      delay(1); // yield
    }
  }

  http.end();

  if (!isChunked && totalRead != contentLen) {
    ESP_LOGI(TAG, "[Download] Incomplete: %d/%d bytes\n", totalRead, contentLen);
    free(buffer);
    return false;
  }

  //ESP_LOGI(TAG, "[Download] Success: %d bytes downloaded to %s\n", totalRead, usedHeap ? "heap" : "PSRAM");
  *outBuffer = buffer;
  *outSize = totalRead;
  return true;
}

// Download SVG file to PSRAM or heap with robust checks
bool mtb_Download_Svg_Img_To_PSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType) {
  //ESP_LOGI(TAG, "[Download SVG] Starting: %s\n", url);

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    ESP_LOGI(TAG, "[Download SVG] HTTP GET failed: %d\n", httpCode);
    http.end();
    return false;
  }

  String contentType = http.header("Content-Type");
  if (outMimeType) *outMimeType = contentType;
  //ESP_LOGI(TAG, "[Download SVG] Content-Type: %s\n", contentType.c_str());

  if (contentType.length() == 0) {
    //ESP_LOGI(TAG, "[Download SVG] Warning: No Content-Type provided. Proceeding anyway...\n");
  } else if (!contentType.startsWith("image/svg") && contentType != "application/octet-stream" && !contentType.startsWith("text/xml")) {
    ESP_LOGI(TAG, "[Download SVG] Unsupported MIME type: %s. Aborting.\n", contentType.c_str());
    http.end();
    return false;
  }

  int contentLen = http.getSize();
  bool isChunked = (contentLen <= 0);
  if (isChunked) ESP_LOGI(TAG, "[Download SVG] Chunked transfer detected.\n");

  const size_t bufferCapacity = isChunked ? 256 * 1024 : contentLen;
  uint8_t* buffer = (uint8_t*)ps_malloc(bufferCapacity);
  bool usedHeap = false;

  if (!buffer) {
    ESP_LOGI(TAG, "[Download SVG] PSRAM allocation failed. Trying heap...\n");
    buffer = (uint8_t*)malloc(bufferCapacity);
    usedHeap = true;
    if (!buffer) {
      ESP_LOGI(TAG, "[Download SVG] Memory allocation failed.\n");
      http.end();
      return false;
    }
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t totalRead = 0;
  uint32_t lastReadTime = millis();

  while (http.connected()) {
    size_t available = stream->available();
    if (available) {
      size_t toRead = min(available, bufferCapacity - totalRead);
      int bytesRead = stream->readBytes(buffer + totalRead, toRead);
      if (bytesRead <= 0) break;
      totalRead += bytesRead;
      lastReadTime = millis();

      if (!isChunked && totalRead >= contentLen) break;
      if (isChunked && totalRead >= bufferCapacity - 512) break;
    } else {
      if (millis() - lastReadTime > 10000) {
        ESP_LOGI(TAG, "[Download SVG] Stream timeout.\n");
        free(buffer);
        http.end();
        return false;
      }
      delay(1);
    }
  }

  http.end();

  if (!isChunked && totalRead != contentLen) {
    ESP_LOGI(TAG, "[Download SVG] Incomplete: %d/%d bytes\n", totalRead, contentLen);
    free(buffer);
    return false;
  }

  //ESP_LOGI(TAG, "[Download SVG] Success: %d bytes downloaded to %s\n", totalRead, usedHeap ? "heap" : "PSRAM");
  *outBuffer = buffer;
  *outSize = totalRead;
  return true;
}
