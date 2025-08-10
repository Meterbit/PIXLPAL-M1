
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "Arduino.h"
#include "mtb_littleFs.h"
#include <time.h>
#include "mtb_ntp.h"
#include "mtb_text_scroll.h"
#include "mtb_audio.h"
#include "mtb_engine.h"
#include "esp_heap_caps.h"
#include <HTTPClient.h>
#include "internet_Radio.h"
#include "mtb_ble.h"

static const char TAG[] = "INTERNET_RADIO";

EXT_RAM_BSS_ATTR TaskHandle_t internet_Radio_Task_H = NULL;

// Default RadioStation
RadioStation_t currentRadioStation = {
    "Naija Hits FM",
    "https://stream.zeno.fm/thbqnu2wvmzuv",
    1
  };

bool radioPlayReady = true;
static const char favouriteRadioStationsFilePath[] = "/radioStations/favSta.csv";
//String posterCurrentStation = "/radioStations/currentPoster.png";
//************************************************************* */
void selectRadioStations(JsonDocument&); // This function is called from the App to select a radio station.
void playRadioStationLink(JsonDocument&);
void volumeControl(JsonDocument&);
void updateSavedStations(JsonDocument&);
//*********************************************************** */
//void station_Poster_Download_Task(void *);
void intRadioButtonControl(button_event_t);
//String getStationInfoAsJson(int);
int searchStationInFavorites(const String&);
bool decodeSaveBase64(String &base64Image, String &fileName);
//void station_Poster_Download(void);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *internetRadio_App = new Mtb_Applications_StatusBar(internetRadio_App_Task, &internet_Radio_Task_H, "Int Rad Task", 10240, pdTRUE);

//***************************************************************************************************
void  internetRadio_App_Task(void* dApplication){
    Mtb_Applications *thisApp = (Mtb_Applications *) dApplication;
    thisApp->mtb_App_EncoderFn_ptr = mtb_Vol_Control_Encoder;
    thisApp->mtb_App_ButtonFn_ptr = intRadioButtonControl;
    mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(selectRadioStations, playRadioStationLink, updateSavedStations, volumeControl);
    mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv, mtb_AudioOut_Sv);
  //**************************************************************************************************************************
    AudioTextTransfer_T audioTextReceiver;
    Mtb_ScrollText_t fmStation(11, 46, 116, CYAN, 15, 20000, Terminal6x8, 20000);
    Mtb_ScrollText_t streamTitle(11, 55, 116, YELLOW, 30, 20000, Terminal6x8, 5000);
    Mtb_ScrollText_t conn2Sta(11, 55, 116, ORANGE_RED, 15, 20000, Terminal6x8, 1000);
    Mtb_FixedText_t internet_Text(6, 14, Terminal8x12);
    Mtb_FixedText_t radio_Text(14, 28, Terminal8x12);

    //statusBarNotif.mtb_Scroll_This_Text("BLUETOOTH LINK DISABLED FOR SEAMLESS AUDIO STREAMING", BLUE_GREEN);
    
    internet_Text.mtb_Write_Colored_String("Internet", FLORAL_WHITE);
    radio_Text.mtb_Write_Colored_String("Radio", FLORAL_WHITE);
    mtb_Draw_Local_Png({"/batIcons/fmRadio.png", 68, 10});
    mtb_Draw_Local_Png({"/batIcons/radStation.png", 2, 46});
    mtb_Draw_Local_Png({"/batIcons/radStrmTitle.png", 2, 55});
    dma_display->drawRect(0, 44, 128, 20, PURPLE_NAVY);
//******************************************************************************************************************************** */
    while (MTB_APP_IS_ACTIVE == pdTRUE){
    mtb_Read_Nvs_Struct("currentRadSta", &currentRadioStation, sizeof(RadioStation_t));

    conn2Sta.mtb_Scroll_This_Text("Awaiting internet connection...", GREEN_LIZARD);
    while(!(Mtb_Applications::internetConnectStatus) && (MTB_APP_IS_ACTIVE == pdTRUE)) vTaskDelay(pdMS_TO_TICKS(500));

    delay(500); //This delay is placed here to allow "audioTextInfo_Q_H" to be created.
    bool cont_To_Host = false;
    conn2Sta.mtb_Scroll_This_Text("Connecting to Station....", ORANGE_RED);
    

    do{
      delay(500);
      cont_To_Host = mtb_audioPlayer->mtb_ConnectToHost(currentRadioStation.streamLink);
    } while((cont_To_Host != true) && (MTB_APP_IS_ACTIVE == pdTRUE));

      //conn2Sta.mtb_Scroll_Active(STOP_SCROLL);
      fmStation.mtb_Scroll_This_Text(currentRadioStation.stationName, CYAN);
      conn2Sta.mtb_Scroll_This_Text("Connected to Radio Station..!!", LEMON_MERINGUE);
      
      radioPlayReady = true;

      while ((Mtb_Applications::internetConnectStatus) && (radioPlayReady) && (MTB_APP_IS_ACTIVE == pdTRUE)){
        if(xQueueReceive(audioTextInfo_Q_H, &audioTextReceiver, 0) == pdTRUE){
          switch(audioTextReceiver.Audio_Text_type){

            case AUDIO_SHOW_STATION:
                fmStation.mtb_Scroll_This_Text(audioTextReceiver.Audio_Text_Data, CYAN);
                //ESP_LOGI(TAG, "Radio Station: %s\n", audioTextReceiver.Audio_Text_Data);
                break;

            case AUDIO_SHOWS_STREAM_TITLE:
                streamTitle.mtb_Scroll_This_Text(audioTextReceiver.Audio_Text_Data, YELLOW);
                //ESP_LOGI(TAG, "Stream Title: %s\n", audioTextReceiver.Audio_Text_Data);
                break;

            default: //ESP_LOGI(TAG, "OTHER INFO: %s\n", audioTextReceiver.Audio_Text_Data);
                break;
          }
        }else if(xSemaphoreTake(audio_Data_Collected_Sem_H, 0) == pdTRUE){
          // audioVisualizer();
          // if(visualize the audio samples)audioVisualizer(&RadioAudioTransferBuffer);
          // if(save radio data to flash drive)
          // if(transmit through bluetooth)
        } else  vTaskDelay(1);
    }
    mtb_Use_Mic_Or_Dac(DISABLE_I2S_MIC_DAC);
}
// fmStation.mtb_Scroll_Active(STOP_SCROLL);
// streamTitle.mtb_Scroll_Active(STOP_SCROLL);
// conn2Sta.mtb_Scroll_Active(STOP_SCROLL);
mtb_End_This_App(thisApp);
}

//##############################################################################################################
// void station_Poster_Download(void){
//   // if((mtb_Draw_Local_Png(currentRadioStation.posterFlashPath.c_str(), 1, 10))){
//   // String downloadSuccess = downloadFireStrgFile(currentRadioStation.posterBucketPath, currentRadioStation.posterFlashPath);
//   // if (downloadSuccess == "Download success") mtb_Draw_Local_Png(currentRadioStation.posterFlashPath.c_str(), 1, 10);
//   // else statusBarNotif.mtb_Scroll_This_Text("STATION POSTER NOT FOUND.", ORANGE); //REPLACE THIS CODE WITH A GENERIC GIF SHOWING STATION NOT FOUND.
//   // }
// }

//##############################################################################################################
void intRadioButtonControl(button_event_t button_Data){
        switch (button_Data.type){
        case BUTTON_RELEASED:
        break;

        case BUTTON_PRESSED:
        break;

        case BUTTON_PRESSED_LONG:
          break;

        case BUTTON_CLICKED:
          switch (button_Data.count){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            default:
                break;
          }
        break;
        default:
        break;
    }
}


//***************************************************************************************************
void selectRadioStations(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  mtb_Ble_App_Cmd_Respond_Success(internetRadioAppRoute, cmd, pdPASS);
}

//************************************************************************************************
void playRadioStationLink(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  const char* streamLink = dCommand["stationLink"];
  const char* stationName = dCommand["stationName"];

  strcpy(currentRadioStation.stationName, stationName); // Copy the station name to the current radio station
  strcpy(currentRadioStation.streamLink, streamLink); // Copy the stream link to the current radio station

  ESP_LOGI(TAG, "Playing Station: %s\n", currentRadioStation.stationName);
  ESP_LOGI(TAG, "Stream Link: %s\n", currentRadioStation.streamLink);

  mtb_Ble_App_Cmd_Respond_Success(internetRadioAppRoute, cmd, pdPASS);
  mtb_Write_Nvs_Struct("currentRadSta", &currentRadioStation, sizeof(RadioStation_t));
  radioPlayReady = false;
}

// Function to write a RadioStation's info to a CSV file
void updateSavedStations(JsonDocument& dCommand){                                 // RECEIVES THE STATION DETAILS AS A JSON.
  uint8_t cmd = dCommand["app_command"];
  mtb_Ble_App_Cmd_Respond_Success(internetRadioAppRoute, cmd, pdPASS);
  // uint8_t stationNumber = dCommand["serialNumber"];
  // String stationName = dCommand["stationName"];
  // String streamLink = dCommand["streamLink"];
  // // String posterPath = dCommand["posterPath"];
  // // String posterLinkInCloud = dCommand["posterLinkInCloud"];

  // uint8_t stationFound = searchStationInFavorites(stationName);

  // if (stationFound) return;
  // else{
  // // Open the CSV file for appending (or create it if it doesn't exist)
  // File csvFile = LittleFS.open(favouriteRadioStationsFilePath, FILE_APPEND);
  
  //     if (!csvFile) {
  //       ESP_LOGI(TAG, "Failed to open file for writing.\n");
  //       return;
  //     }
  //     // Write the RadioStation information to the CSV file
  //     csvFile.print(stationNumber);
  //     csvFile.print(",");
  //     csvFile.print(stationName);
  //     csvFile.print(",");
  //     csvFile.println(streamLink);
  //     // csvFile.print(",");
  //     // csvFile.println(posterPath);
  //     // csvFile.print(",");
  //     // csvFile.println(posterLinkInCloud);
  //     // Close the file
  //     csvFile.close();
  //     ESP_LOGI(TAG, "Station info written to CSV file.\n");
  //   }
}

//************************************************************************************************
void volumeControl(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  uint8_t volumeLevel = dCommand["volume"];

  audio->setVolume(volumeLevel);
  mtb_Write_Nvs_Struct("dev_Volume", &volumeLevel, sizeof(uint8_t));

  mtb_Ble_App_Cmd_Respond_Success(internetRadioAppRoute, cmd, pdPASS);
}

// //************************************************************************************************
// // Function to retrieve a Radio Station entry by its serial number and return it as a JSON string
// String getStationInfoAsJson(int serialNumber) {

//   // Open the CSV file for reading
//   File file = LittleFS.open(favouriteRadioStationsFilePath, FILE_READ);
//   if (!file) {
//     ESP_LOGI(TAG, "Failed to open file for reading\n");
//     return "";
//   }

//   // Read the file line by line to find the matching serial number
//   while (file.available()) {
//     String line = file.readStringUntil('\n');
//     int foundSerialNumber = line.substring(0, line.indexOf(',')).toInt();
//     if (foundSerialNumber == serialNumber) {
//       // If the serial number matches, parse the line and create a JSON object
//       JsonDocument doc(1024);
//       // Splitting the CSV line into parts
//       int startIndex = 0, endIndex = 0;
//       endIndex = line.indexOf(',', startIndex);
//       doc["serialNumber"] = line.substring(startIndex, endIndex).toInt();

//       startIndex = endIndex + 1;
//       endIndex = line.indexOf(',', startIndex);
//       doc["stationName"] = line.substring(startIndex, endIndex);

//       startIndex = endIndex + 1;
//       endIndex = line.indexOf(',', startIndex);
//       doc["streamLink"] = line.substring(startIndex, endIndex);

//       // startIndex = endIndex + 1;
//       // endIndex = line.length();
//       // doc["posterPath"] = line.substring(startIndex, endIndex);

//       // Convert JSON object to String
//       String jsonString;
//       serializeJson(doc, jsonString);
//       file.close();
//       return jsonString;
//     }
//   }
//   // Close the file if the serial number is not found
//   file.close();
//   // Return an empty string if no matching station is found
//   return "";
// }

// // Function to search for a station by name and return its serial number
// int searchStationInFavorites(const String& stationName) {

//   // Open the CSV file for reading
//   File file = LittleFS.open(favouriteRadioStationsFilePath, FILE_READ);
//   if (!file) {
//     ESP_LOGI(TAG, "Failed to open file for reading\n");
//     return 0;
//   }

//   while (file.available()) {
//     String line = file.readStringUntil('\n');
//     int firstCommaIndex = line.indexOf(',');
//     int secondCommaIndex = line.indexOf(',', firstCommaIndex + 1);
//     String currentStationName = line.substring(firstCommaIndex + 1, secondCommaIndex);

//     // Compare the current station name with the provided station name
//     if (currentStationName.equalsIgnoreCase(stationName)) {
//       // If found, extract the serial number and return it
//       String serialNumberStr = line.substring(0, firstCommaIndex);
//       int serialNumber = serialNumberStr.toInt();
//       file.close();
//       return serialNumber;
//     }
//   }

//   // Close the file and return 0 if the station name is not found
//   file.close();
//   return 0;
// }


// // Function to remove a Radio Station entry by its serial number
// void removeFromFavourites(JsonDocument& dCommand){               //RECEIVES THE STATION SERIAL NUMBER.

//   int serialNumber = dCommand["serialNumber"];

//   // Open the original CSV file for reading
//   File originalFile = LittleFS.open(favouriteRadioStationsFilePath, FILE_READ);
//   if (!originalFile) {
//     ESP_LOGI(TAG, "Failed to open file for reading\n");
//     return;
//   }

//   // Create a temporary file for the new CSV content
//   String tempFileName = "/temp_stations.csv";
//   File tempFile = LittleFS.open(tempFileName, FILE_WRITE);
//   if (!tempFile) {
//     ESP_LOGI(TAG, "Failed to open temporary file for writing \n");
//     originalFile.close(); // Make sure to close the original file before returning
//     return;
//   }

//   // Read the original file line by line and write to the temporary file if the serial number does not match
//   while (originalFile.available()) {
//     String line = originalFile.readStringUntil('\n');
//     // Split the line by comma and check the first value (serial number)
//     int lineSerialNumber = line.substring(0, line.indexOf(',')).toInt();
//     if (lineSerialNumber != serialNumber) {
//       tempFile.println(line); // Write to temp file if serial number does not match
//     }
//   }

//   // Close both files before renaming
//   originalFile.close();
//   tempFile.close();

//   // Remove the original file
//   LittleFS.remove(favouriteRadioStationsFilePath);
//   // Rename the temporary file to the original file name
//   LittleFS.rename(tempFileName, favouriteRadioStationsFilePath);

//   ESP_LOGI(TAG, "Station entry removed.\n");
// }

//************************************************************************************************

// void playRadioStationNew(JsonDocument& dCommand){ // Play by receiving station name from App.
//   RadioStation_t radioStation;
//   radioStation.stationName = dCommand["stationName"].as<String>();

//   uint8_t stationFoundInFav = searchStationInFavorites(radioStation.stationName);

//   if (stationFoundInFav){
//     dCommand["serialNumber"] = stationFoundInFav;
//     playRadioStationNumber(dCommand);
//     return;
//     }else{
//           radioStation.streamLink = dCommand["streamLink"].as<String>();
//           //String posterLinkOnline = dCommand["posterLinkOnline"].as<String>();

//       // // Download the file and save it to LittleFS
//       // if (downloadStationPoster(posterLinkOnline, posterCurrentStation)) {
//       //   ESP_LOGI(TAG, "File successfully downloaded and saved to LittleFS.");
//       //   // You can now do something with the file.
//       // } else {
//       //   ESP_LOGI(TAG, "Failed to download or save the file.\n");
//       // }
//       //currentRadioStation.posterFlashPath = posterCurrentStation;
//       currentRadioStation = radioStation;

//       mtb_Write_Nvs_Struct("currentRadSta", &currentRadioStation, sizeof(RadioStation_t));

//       radioPlayReady = false;
//     }
// }