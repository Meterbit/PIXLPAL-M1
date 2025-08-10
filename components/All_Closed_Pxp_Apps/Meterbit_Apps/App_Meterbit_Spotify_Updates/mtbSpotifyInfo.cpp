#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "LittleFS.h"
//#include "mtb_graphics.h"
#include "SpotifyAuth.h"
#include "SpotifyPlayback.h"
#include "mtbSpotifyInfo.h"

static const char *TAG = "PXP_SPOTIFY";

char* spotify_root_ca;

static const char client_id[] = "3086b005f2b9432d9a6a4222c28f4b87";     // Your client ID of your spotify APP
// static const char clientSecret[] = "a67d4bb0b6874677b4f8cceb273cdaaa"; // Your client Secret of your spotify APP (Do Not share this!)


//*************************************************************************************************** */

Spotify_Data_t userSpotify = {
    "no_Refresh_Token_Saved_Yet"
};

TaskHandle_t spotify_Task_H = NULL;
TaskHandle_t screenUpdates_Task_H = NULL; 
void spotify_App_Task(void *);
void performScreenUpdate_Task( void * pvParameters );

//*************************************************************************************************** */
void link_Spotify(JsonDocument&);
void get_Spotify_Refresh_Token(JsonDocument&);

//Mtb_Services *spotifyScreenUpdate_Sv = new Mtb_Services(performScreenUpdate_Task, &screenUpdates_Task_H, "screenUpdates", 10240, pdTRUE);
Mtb_Applications_StatusBar *spotify_App = new Mtb_Applications_StatusBar(spotify_App_Task, &spotify_Task_H, "spotify App", 10240); // Review down this stack size later.

//THIS IS THE APPLICATION IMPLEMENTATION ***************************************************************************
void  spotify_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *) dApplication;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(link_Spotify, get_Spotify_Refresh_Token);
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
//**************************************************************************************************************************************************************** */

  // Open the .txt file from SPIFFS
  File file = LittleFS.open("/rootCert/spotifyCA.crt", "r");
  if (!file) {
    ESP_LOGI(TAG, "Failed to open file for reading\n");
    return;
  }

  // Get the size of the file
  size_t fileSize = file.size();
  ESP_LOGI(TAG, "File size: %d bytes\n", fileSize);

  // Allocate a buffer in PSRAM
  char* buffer = (char*)heap_caps_malloc(fileSize + 1, MALLOC_CAP_SPIRAM);
  if (buffer == NULL) {
    ESP_LOGI(TAG, "Failed to allocate PSRAM\n");
    file.close();
    return;
  }

  // Read the file content into the buffer
  size_t bytesRead = file.readBytes(buffer, fileSize);
  buffer[bytesRead] = '\0';  // Null-terminate the string

  ESP_LOGI(TAG, "Read %d bytes from file\n", bytesRead);

  // // Optionally, print the file content
  // ESP_LOGI(TAG, "The String is: \n\n%s", buffer);

  //Close the file
  file.close();

  spotify_root_ca = buffer;

//######################################################################################### */
  while (MTB_APP_IS_ACTIVE == pdTRUE){

  while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)) delay(1000);
  
  mtb_Read_Nvs_Struct("spotifyData", &userSpotify, sizeof(Spotify_Data_t));
  ESP_LOGI(TAG, "The refreshToken is: %s\n", userSpotify.refreshToken);

  if (getAccessToken(client_id, userSpotify.refreshToken)) {
    getNowPlaying();
    //ESP_LOGI(TAG, "I'm ready to get the now playing song.\n");
  }

    while (MTB_APP_IS_ACTIVE == pdTRUE){
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "pause") {
      sendPlaybackCommand("pause", "PUT");
    } else if (cmd == "play") {
      sendPlaybackCommand("play", "PUT");
    } else if (cmd == "next") {
      sendPlaybackCommand("next", "POST");
    } else if (cmd == "info") {
      getNowPlaying();
    }
  }

  delay(100);
    }

  }

  mtb_End_This_App(thisApp);
}


void performScreenUpdate_Task( void * d_Service ){
      Mtb_Services *thisServ = (Mtb_Services *)d_Service;

    while(MTB_SERV_IS_ACTIVE == pdTRUE){
          //server.handleClient();
    }
    mtb_End_This_Service(thisServ);
}

//***************************************************************************************************
void link_Spotify(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  mtb_Ble_App_Cmd_Respond_Success(spotifyAppRoute, cmd, pdPASS);
}

// This function is called when the Spotify refresh token is received
void get_Spotify_Refresh_Token(JsonDocument& dCommand){
uint8_t cmd = dCommand["app_command"];
const char *refreshToken = dCommand["refreshToken"];
strcpy(userSpotify.refreshToken, refreshToken);
mtb_Write_Nvs_Struct("spotifyData", &userSpotify, sizeof(Spotify_Data_t));
statusBarNotif.mtb_Scroll_This_Text("SPOTIFY LINK UPDATED. YOU MAY CLOSE THE BROWSER", GREEN);
//ESP_LOGI(TAG, "Refresh Token: %s\n", userSpotify.refreshToken);
mtb_Ble_App_Cmd_Respond_Success(spotifyAppRoute, cmd, pdPASS);
}