
#include "mtbGithubStorage.h"
#include "mtbApps.h"
#include "scrollMsgs.h"
#include "mtb_ghota.h"
#include <WiFi.h>
#include <HTTPClient.h>

EXT_RAM_BSS_ATTR QueueHandle_t files2Download_Q = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t files2Download_Task_H = NULL;
void files2Download_Task(void*);

EXT_RAM_BSS_ATTR Services *gitHubFileDwnload_Sv = new Services(files2Download_Task, &files2Download_Task_H, "Github Dwnld", 10240, 2, pdFALSE, 1);

bool downloadGithubStrgFile(String bucketPath, String flashPath){
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

  //printf("Requesting URL: %s \n", url.c_str());

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
    // printf("The github file path is: %s \n", flashPath.c_str());

    if (prepareFilePath(flashPath.c_str())){
      file = LittleFS.open(flashPath, FILE_WRITE);
      if (!file)
      {
        printf("Failed to open file for writing.\n");
        https.end();
        return false;
      }
    } else {
      printf("File Path preparation failed.\n");
    }

    // Get the response stream
    WiFiClient * stream = https.getStreamPtr();

    // Read data from stream and write to file
    uint8_t buff[128] = { 0 };
    int len = https.getSize();
    //printf("Filesize length: %d\n", len);

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
    //printf("File downloaded and saved to LittleFS\n");
  } else {
    Serial.printf("HTTP Error: %d\n", httpCode);
  }

  https.end();
  return true;
}

bool downloadGithubStrgFile(githubStrg_UpDwn_t& downloadPaths){
    return downloadGithubStrgFile(downloadPaths.bucketFilePath, downloadPaths.flashFilePath);
}

void files2Download_Task(void* dService){
	Services *thisService = (Services*) dService;
  File2Download_t holderItem;
  bool dwnld_Succeed = false;

  while (xQueueReceive(files2Download_Q, &holderItem, pdMS_TO_TICKS(100)) == pdTRUE){
    statusBarNotif.scroll_This_Text("UPDATING FILES", GREEN);
    dwnld_Succeed = downloadGithubStrgFile(String(holderItem.githubFilePath), String(holderItem.flashFilePath));
    if(dwnld_Succeed) statusBarNotif.scroll_This_Text("FILE STORAGE UPDATE SUCCESSFUL", SANDY_BROWN);
  }

  Applications::internetConnectStatus = true;
	kill_This_Service(thisService);
}


bool prepareFilePath(const char* filePath) {
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
          printf("Failed to create directory: %s\n", subPath.c_str());
          return false;
        }
      }
    }
  }

  // Create final directory level if needed
  if (!LittleFS.exists(dirPath)) {
    if (!LittleFS.mkdir(dirPath)) {
      printf("Failed to create directory: %s\n", dirPath.c_str());
      return false;
    }
  }

  return true;
}


// Example usage:
// downloadImageToSPIFFS("https://media.api-sports.io//football//teams//45.png", "/team_45.png");

// bool downloadOnlineImageToSPIFFS(const char* url, const char* pathInSPIFFS) {
//   printf("Downloading: %s\n", url);
//   File file;
//   HTTPClient http;
//   http.begin(url);
//   int httpCode = http.GET();

//   if (httpCode != HTTP_CODE_OK) {
//     printf("HTTP GET failed, error: %d\n", httpCode);
//     http.end();
//     return false;
//   }

//   if (prepareFilePath(pathInSPIFFS)){

// 	  file = LittleFS.open(pathInSPIFFS, FILE_WRITE);
// 	  if (!file){
// 		  printf("Failed to open file for writing\n");
// 		  http.end();
// 		  return false;
// 	  }
//   } else {
// 	  printf("File Path preparation failed.\n");
//   }

//   int total = http.writeToStream(&file);


//   file.close();
//   http.end();

//   printf("File saved to %s (%d bytes)\n", pathInSPIFFS, total);
//   return true;
// }

// bool downloadOnlineImageToSPIFFS(const char* url, const char* pathInSPIFFS) {
//   printf("Downloading: %s\n", url);
//   File file;
//   HTTPClient http;
//   http.begin(url);
//   int httpCode = http.GET();

//   if (httpCode != HTTP_CODE_OK) {
//     printf("HTTP GET failed, error: %d\n", httpCode);
//     http.end();
//     return false;
//   }

//   // Check for valid content-type
//   String contentType = http.header("Content-Type");
//   printf("Content-Type: %s\n", contentType.c_str());

//   // Allowed MIME types (add more if needed)
//   bool validContent =
//     contentType.startsWith("image/") ||
//     contentType == "application/octet-stream" ||
//     contentType == "application/pdf";

//   if (!validContent) {
//     printf("Blocked download: Unexpected content type: %s\n", contentType.c_str());
//     http.end();
//     return false;
//   }

//   if (prepareFilePath(pathInSPIFFS)) {
//     file = LittleFS.open(pathInSPIFFS, FILE_WRITE);
//     if (!file) {
//       printf("Failed to open file for writing\n");
//       http.end();
//       return false;
//     }
//   } else {
//     printf("File path preparation failed.\n");
//     http.end();
//     return false;
//   }

//   int total = http.writeToStream(&file);

//   file.close();
//   http.end();

//   printf("File saved to %s (%d bytes)\n", pathInSPIFFS, total);
//   return (total > 0);
// }


// Download image to PSRAM or heap with auto fallback, timeout, type detection, and support for chunked transfer
bool downloadPNGImageToPSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType) {
  //printf("[Download] Starting: %s\n", url);

  HTTPClient http;
  http.setTimeout(15000); // 15-second timeout
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    //printf("[Download] HTTP GET failed: %d\n", httpCode);
    http.end();
    return false;
  }

  String contentType = http.header("Content-Type");
  if (outMimeType) *outMimeType = contentType;
  //printf("[Download] Content-Type: %s\n", contentType.c_str());

if (contentType.length() == 0) {
  //printf("[Download] Warning: No Content-Type provided. Proceeding anyway...\n");
} else if (!contentType.startsWith("image/") && contentType != "application/octet-stream") {
  //printf("[Download] Unsupported MIME type: %s. Aborting.\n", contentType.c_str());
  http.end();
  return false;
}

  int contentLen = http.getSize();
  bool isChunked = (contentLen <= 0);
  if (isChunked) printf("[Download] Chunked transfer detected.\n");

  const size_t bufferCapacity = isChunked ? 256 * 1024 : contentLen; // Allocate 256KB for chunked by default
  uint8_t* buffer = (uint8_t*)ps_malloc(bufferCapacity);
  bool usedHeap = false;

  if (!buffer) {
    printf("[Download] PSRAM allocation failed. Trying heap...\n");
    buffer = (uint8_t*)malloc(bufferCapacity);
    usedHeap = true;
    if (!buffer) {
      printf("[Download] Memory allocation failed.\n");
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
        printf("[Download] Stream timeout.\n");
        free(buffer);
        http.end();
        return false;
      }
      delay(1); // yield
    }
  }

  http.end();

  if (!isChunked && totalRead != contentLen) {
    printf("[Download] Incomplete: %d/%d bytes\n", totalRead, contentLen);
    free(buffer);
    return false;
  }

  //printf("[Download] Success: %d bytes downloaded to %s\n", totalRead, usedHeap ? "heap" : "PSRAM");
  *outBuffer = buffer;
  *outSize = totalRead;
  return true;
}

// Download SVG file to PSRAM or heap with robust checks
bool downloadSVGImageToPSRAM(const char* url, uint8_t** outBuffer, size_t* outSize, String* outMimeType) {
  printf("[Download SVG] Starting: %s\n", url);

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    printf("[Download SVG] HTTP GET failed: %d\n", httpCode);
    http.end();
    return false;
  }

  String contentType = http.header("Content-Type");
  if (outMimeType) *outMimeType = contentType;
  printf("[Download SVG] Content-Type: %s\n", contentType.c_str());

  if (contentType.length() == 0) {
    printf("[Download SVG] Warning: No Content-Type provided. Proceeding anyway...\n");
  } else if (!contentType.startsWith("image/svg") && contentType != "application/octet-stream" && !contentType.startsWith("text/xml")) {
    printf("[Download SVG] Unsupported MIME type: %s. Aborting.\n", contentType.c_str());
    http.end();
    return false;
  }

  int contentLen = http.getSize();
  bool isChunked = (contentLen <= 0);
  if (isChunked) printf("[Download SVG] Chunked transfer detected.\n");

  const size_t bufferCapacity = isChunked ? 256 * 1024 : contentLen;
  uint8_t* buffer = (uint8_t*)ps_malloc(bufferCapacity);
  bool usedHeap = false;

  if (!buffer) {
    printf("[Download SVG] PSRAM allocation failed. Trying heap...\n");
    buffer = (uint8_t*)malloc(bufferCapacity);
    usedHeap = true;
    if (!buffer) {
      printf("[Download SVG] Memory allocation failed.\n");
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
        printf("[Download SVG] Stream timeout.\n");
        free(buffer);
        http.end();
        return false;
      }
      delay(1);
    }
  }

  http.end();

  if (!isChunked && totalRead != contentLen) {
    printf("[Download SVG] Incomplete: %d/%d bytes\n", totalRead, contentLen);
    free(buffer);
    return false;
  }

  printf("[Download SVG] Success: %d bytes downloaded to %s\n", totalRead, usedHeap ? "heap" : "PSRAM");
  *outBuffer = buffer;
  *outSize = totalRead;
  return true;
}
