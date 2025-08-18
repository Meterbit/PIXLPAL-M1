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
#include "mtb_ntp.h"
#include "mtb_engine.h"
#include "mtb_text_scroll.h"
#include "mtb_audio.h"
#include <OpenAI.h>
#include "LittleFS.h"
#include "esp_heap_caps.h"
#include "mtb_ble.h"
#include "pxp_secret_keys.h"

//#define NUM_CHANNELS 1
#define REC_DURATION_SECONDS_MAX 15
#define REC_DURATION_BYTES_MAX 500000  // 15+ SECONDS, 16BIT, 16000HZ


static const char *TAG = "PIXP_CHAT_GPT";

    struct psRamWavFileChatGPT{
    uint8_t* psRamFile_P;
    size_t psRamFile_Size;
};

EXT_RAM_BSS_ATTR int16_t *totalSampleBuffer = NULL;
EXT_RAM_BSS_ATTR int8_t recordingCountdown = 0;
EXT_RAM_BSS_ATTR TimerHandle_t chatPromptTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t micAudioListen_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t chatGPT_Task_H = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t chatPrompt_Queue_H = NULL;
void micAudioListen_Task(void *);
void chatGPT_App_Task(void *);
void chatPrompt_TimerCallback(TimerHandle_t);

uint8_t *create_wav_in_psram(const int16_t *buffer, uint32_t numSamples, uint32_t sampleRate, size_t *wavSize);
//uint8_t* convertSamples2Wav(int16_t *audioSamples, int numSamples, size_t*);
void Listen_Process_Button(button_event_t);

// void selectNumOfBands(JsonDocument&);
// void selectPattern(JsonDocument&);
// void setRandomPatterns(JsonDocument&);
// void setSensitivity(JsonDocument&);

  Mtb_ScrollText_t* humanSpeech;
  Mtb_ScrollText_t* aiResponse;
  //Mtb_FixedText_t recordCountdownText(116, 45, Terminal6x8);


void delete_file(const char *path) {
    if (unlink(path) == 0) {
        ESP_LOGI(TAG, "File deleted successfully\n");
    } else {
        ESP_LOGI(TAG, "File deletion failed\n");
    }
}

OpenAI openai(openai_key);
OpenAI_ChatCompletion chat(openai);


EXT_RAM_BSS_ATTR Mtb_Services *Audio_Listening_Sv = new Mtb_Services(micAudioListen_Task, &micAudioListen_Task_H, "Aud listen Serv", 4096);
EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *chatGPT_App = new Mtb_Applications_StatusBar(chatGPT_App_Task, &chatGPT_Task_H, "chatGPT App", 10240); // Review down this stack size later.

//***************************************************************************************************
void  chatGPT_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *) dApplication;
  psRamWavFileChatGPT recordWavFile;
  Mtb_FixedText_t chatGPT_Text(46, 19, Terminal10x17);
  Mtb_ScrollText_t conn2Intnt(11, 55, 116, ORANGE_RED, 15, 20000, Terminal6x8, 1000);
  thisApp->mtb_App_EncoderFn_ptr = mtb_Vol_Control_Encoder;
  thisApp->mtb_App_ButtonFn_ptr = Listen_Process_Button;
  //appMQTT_Parser_Sv->register_MqttServiceFns(selectPattern, selectNumOfBands, setRandomPatterns, setSensitivity);
  mtb_App_Init(thisApp, mtb_Mic_Sv, mtb_Status_Bar_Clock_Sv, mtb_AudioOut_Sv);
  //**************************************************************************************************************************
  humanSpeech = new Mtb_ScrollText_t (11, 45, 116, CYAN, 25, 20000, Terminal6x8, 0);
  aiResponse = new Mtb_ScrollText_t(11, 55, 116, YELLOW, 15, 3, Terminal6x8, 0);

  if(chatPrompt_Queue_H == NULL) chatPrompt_Queue_H = xQueueCreate(3, sizeof(psRamWavFileChatGPT));
  if(chatPromptTimer_H == NULL) chatPromptTimer_H = xTimerCreate("chatPrompt Timer", pdMS_TO_TICKS(1000), true, NULL, chatPrompt_TimerCallback);
  if(totalSampleBuffer == NULL) totalSampleBuffer = (int16_t* ) heap_caps_malloc(REC_DURATION_BYTES_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  mtb_Draw_Local_Png({"/batIcons/openai.png", 12, 12});
  chatGPT_Text.mtb_Write_Colored_String("ChatGPT", WHITE);
  mtb_Draw_Local_Png({"/batIcons/aiResp.png", 2, 45});
  mtb_Draw_Local_Png({"/batIcons/hmanSph.png", 2, 55});
  dma_display->drawRect(0, 43, 128, 21, PURPLE_NAVY);
  
//************************************************************************************* */
  chat.setModel("gpt-3.5-turbo");   //Model to use for completion. Default is gpt-3.5-turbo
  chat.setSystem("You are a helpful assistant"); //Description of the required assistant
  chat.setMaxTokens(2048);          //The maximum number of tokens to generate in the completion.
  chat.setTemperature(0.2);         //float between 0 and 2. Higher value gives more random results.
  chat.setStop("\r");               //Up to 4 sequences where the API will stop generating further tokens.
  chat.setPresencePenalty(0);       //float between -2.0 and 2.0. Positive values increase the model's likelihood to talk about new topics.
  chat.setFrequencyPenalty(0);      //float between -2.0 and 2.0. Positive values decrease the model's likelihood to repeat the same line verbatim.
  chat.setUser("PIXLPAL");          //A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
//********************************************************************************** */

  OpenAI_AudioTranscription audioTranscription(openai);
  audioTranscription.setResponseFormat(OPENAI_AUDIO_RESPONSE_FORMAT_JSON);
  audioTranscription.setTemperature(0.2);  //float between 0 and 1. Higher value gives more random results.
  audioTranscription.setLanguage("en"); 

  delay(500); //This delay is placed here to allow "audioTextInfo_Q_H" to be created.

//######################################################################################### */
    while (MTB_APP_IS_ACTIVE == pdTRUE){

    conn2Intnt.mtb_Scroll_This_Text("Awaiting internet connection...", GREEN_LIZARD);
    while((Mtb_Applications::internetConnectStatus == false) && (MTB_APP_IS_ACTIVE == pdTRUE)) vTaskDelay(pdMS_TO_TICKS(500));
    conn2Intnt.mtb_Scroll_Active(STOP_SCROLL);
    humanSpeech->mtb_Scroll_This_Text("Press the knob and speak.");

    while (MTB_APP_IS_ACTIVE == pdTRUE && (Mtb_Applications::internetConnectStatus == true) ){
    if(xQueueReceive(chatPrompt_Queue_H, &recordWavFile, pdMS_TO_TICKS(50)) != pdTRUE) continue;
    //delay(1000);
    // audio->connecttoFS(LittleFS, "/audio.wav"); // LittleFS
    // mtb_Use_Mic_Or_Dac(I2S_DAC);
    // ESP_LOGI(TAG, "In the ChatGPT App Again...\n");
    // delete_file("/littlefs/audio.wav");
    // heap_caps_free(recordWavFile.psRamFile_P);
    //*************************************************************************************** */
    String text = audioTranscription.file(recordWavFile.psRamFile_P, recordWavFile.psRamFile_Size, OPENAI_AUDIO_INPUT_FORMAT_WAV);
    ESP_LOGI(TAG, "Text: %s", text.c_str());

    String voiceText = text;
    humanSpeech->mtb_Scroll_This_Text(voiceText.c_str());
    // // //*************************************************************************************** */
    heap_caps_free(recordWavFile.psRamFile_P);
    OpenAI_StringResponse result = chat.message(text);
      if(result.length() == 1){
          //ESP_LOGI(TAG, "Received message. Tokens: %u\n", result.tokens());
          String response = result.getAt(0);
          response.trim();
          //ESP_LOGI(TAG, "%s", response.c_str());
        mtb_audioPlayer->mtb_Openai_Speech("tts-1", response , "shimmer", "mp3", "1");
        aiResponse->mtb_Scroll_This_Text(response);
        
        } else if(result.length() > 1){
          //ESP_LOGI(TAG, "Received %u messages. Tokens: %u\n", result.length(), result.tokens());
          for (unsigned int i = 0; i < result.length(); ++i){
            String response = result.getAt(i);
            response.trim();
            //ESP_LOGI(TAG, "Message[%u]:\n%s\n", i, response.c_str());
          }
        } else if(result.error()){
          Serial.print("Error! ");
          //ESP_LOGE(TAG, "Error! %s \n", result.error());
        } else {
           //ESP_LOGE(TAG, "Unknown error!");
        }
  //**************************************************************************************** */
    }
  }
  
  // Memory Cleanup Starts Here.
  chat.clearConversation();
  free(totalSampleBuffer); totalSampleBuffer = NULL;
  vQueueDelete(chatPrompt_Queue_H); chatPrompt_Queue_H = NULL;
  xTimerDelete(chatPromptTimer_H, 0); chatPromptTimer_H = NULL;
  conn2Intnt.mtb_Scroll_Active(STOP_SCROLL);
  aiResponse->mtb_Scroll_Active(STOP_SCROLL);
  humanSpeech->mtb_Scroll_Active(STOP_SCROLL);
  Audio_Listening_Sv->service_is_Running = pdFALSE;

  delete humanSpeech;
  delete aiResponse;

  mtb_End_This_App(thisApp);
}

void Listen_Process_Button(button_event_t button_Data){
    switch (button_Data.type){
    case BUTTON_RELEASED:       
    break;

    case BUTTON_PRESSED:
    if(Mtb_Applications::internetConnectStatus == true){
       if(*(Audio_Listening_Sv->serviceT_Handle_ptr) == NULL){
        mtb_Use_Mic_Or_Dac(DISABLE_I2S_MIC_DAC); 
        xTimerStart(chatPromptTimer_H, 0);
        aiResponse->mtb_Scroll_Active(STOP_SCROLL);
        humanSpeech->mtb_Scroll_Active(STOP_SCROLL);
        mtb_Start_This_Service(Audio_Listening_Sv);
        recordingCountdown = REC_DURATION_SECONDS_MAX;
        mtb_Draw_Local_Png({"/batIcons/micRec.png", 2, 45});
      } else {
        xTimerStop(chatPromptTimer_H, 0);
        mtb_Use_Mic_Or_Dac(DISABLE_I2S_MIC_DAC);
        Audio_Listening_Sv->service_is_Running = pdFALSE;
        mtb_Draw_Local_Png({"/batIcons/aiResp.png", 2, 45});
      }} else {
      statusBarNotif.mtb_Scroll_This_Text("NO INTERNET CONNECTION. TRY AGAIN LATER.", ORANGE);
      }
      break;

    case BUTTON_PRESSED_LONG:
    break;

    case BUTTON_CLICKED:
    //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
    switch (button_Data.count){
    case 1:
        break;
    case 2:
        chat.clearConversation();
        aiResponse->mtb_Scroll_This_Text("Conversation cleared.");
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

// WAV file header structure
typedef struct {
    char chunkID[4];     // "RIFF"
    uint32_t chunkSize;  // Size of the whole file minus 8 bytes
    char format[4];      // "WAVE"
    char subChunk1ID[4]; // "fmt "
    uint32_t subChunk1Size; // 16 for PCM
    uint16_t audioFormat;   // PCM = 1
    uint16_t numChannels;   // Mono = 1
    uint32_t sampleRate;    // Sample rate
    uint32_t byteRate;      // SampleRate * NumChannels * BitsPerSample/8
    uint16_t blockAlign;    // NumChannels * BitsPerSample/8
    uint16_t bitsPerSample; // Bits per sample
    char subChunk2ID[4];    // "data"
    uint32_t subChunk2Size; // NumSamples * NumChannels * BitsPerSample/8
} WAVHeader;

//######################################################################################### */
void micAudioListen_Task(void* d_Service){	// Consider using hardware timer for updating this function to save processor space/time.
	Mtb_Services *thisServ = (Mtb_Services *)d_Service;
  psRamWavFileChatGPT recWave;
  Mtb_FixedText_t recordingVoice(11, 45, Terminal6x8);
  uint32_t wavSize;
  int totalSamples = 0;
  recordingVoice.mtb_Write_Colored_String("Listening...", LEMON);
  mtb_Use_Mic_Or_Dac(I2S_MIC);
  while (MTB_SERV_IS_ACTIVE == pdTRUE && xTimerIsTimerActive(chatPromptTimer_H) == pdTRUE){
		if(xSemaphoreTake(audio_Data_Collected_Sem_H, pdMS_TO_TICKS(5))){
    AudioSamplesTransport.audioSampleLength_bytes /= 2;
    for (uint16_t i = 0; i < AudioSamplesTransport.audioSampleLength_bytes; i++){
      totalSampleBuffer[totalSamples++] = AudioSamplesTransport.audioBuffer[i];
      //ESP_LOGI(TAG, "Samp Recvd\n");
      }
    }
  }
  recordingVoice.mtb_Write_Colored_String("Processing...", GREEN);
  mtb_Draw_Local_Png({"/batIcons/aiResp.png", 2, 45});
  recWave.psRamFile_P = create_wav_in_psram(totalSampleBuffer, totalSamples, SAMPLE_RATE, &(recWave.psRamFile_Size));
  xQueueSend(chatPrompt_Queue_H, &recWave, pdTICKS_TO_MS(250));
  mtb_End_This_Service(thisServ);
  }
//######################################################################################### */

void chatPrompt_TimerCallback(TimerHandle_t chatPrompt){
  //String number = String(--recordingCountdown);

  if(recordingCountdown --> 0){
    //recordCountdownText.mtb_Write_Colored_String(number.c_str(), LEMON);
    if(recordingCountdown % 2){
      mtb_Draw_Local_Png({"/batIcons/micRec.png", 2, 45});
    } 
    else {
      mtb_Draw_Local_Png({"/batIcons/wipe7x7.png", 2,45});
    }
  } else{
    Audio_Listening_Sv->service_is_Running = pdFALSE;
    mtb_Use_Mic_Or_Dac(DISABLE_I2S_MIC_DAC);
    xTimerStop(chatPromptTimer_H, 0);
  }
}

//######################################################################################### */

// Function to generate a WAV header
WAVHeader generate_wav_header(uint32_t numSamples) {
    WAVHeader header;

    memcpy(header.chunkID, "RIFF", 4);
    header.chunkSize = 36 + numSamples * 2;  // 2 bytes per sample
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subChunk1ID, "fmt ", 4);
    header.subChunk1Size = 16;
    header.audioFormat = 1;  // PCM
    header.numChannels = 1;  // Mono
    header.sampleRate = SAMPLE_RATE;
    header.byteRate = SAMPLE_RATE * 2;  // SampleRate * NumChannels * BitsPerSample/8
    header.blockAlign = 2;  // NumChannels * BitsPerSample/8
    header.bitsPerSample = 16;  // 16 bits
    memcpy(header.subChunk2ID, "data", 4);
    header.subChunk2Size = numSamples * 2;  // NumSamples * NumChannels * BitsPerSample/8

    return header;
}

// Function to create a WAV file in PSRAM
uint8_t* create_wav_in_psram(const int16_t* buffer, uint32_t numSamples, uint32_t sampleRate, size_t* wavSize) {
    // Calculate the total size of the WAV file
    uint32_t totalSize = sizeof(WAVHeader) + numSamples * sizeof(int16_t);

    // Allocate memory in PSRAM
    uint8_t* wavFile = (uint8_t*) heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM);
    if (!wavFile) {
        ESP_LOGI(TAG, "Failed to allocate PSRAM for WAV file.\n");
        return NULL;
    }

    // Generate WAV header
    WAVHeader dHeader = generate_wav_header(numSamples);

    // Copy header and audio samples to the WAV file buffer
    memcpy(wavFile, &dHeader, sizeof(WAVHeader));
    memcpy(wavFile + sizeof(WAVHeader), buffer, numSamples * sizeof(int16_t));

    // Set the output WAV file size
    *wavSize = totalSize;

    return wavFile;
}

// // Function to save a WAV file
// uint8_t* convertSamples2Wav(int16_t* audioSamples, int numSamples, size_t* wavSize) {

//     uint8_t* wavFile = create_wav_in_psram(audioSamples, numSamples, SAMPLE_RATE, wavSize);
//     if (wavFile) {
//       // The wavFile now contains the WAV file data in PSRAM
//       //ESP_LOGI(TAG, "WAV file created in PSRAM, size: %d \n", *wavSize);
//       // Don't forget to free the allocated PSRAM when done


// // // Open a file for writing
// //   File file = LittleFS.open("/audio.wav", FILE_WRITE);

// //   if (!file) {
// //     ESP_LOGI(TAG, "Failed to open file for writing.\n");
// //     return 0;
// //   }

// //   // Write the data from RAM to the file
// //   size_t written = file.write(wavFile, *wavSize);

// //   if (written != (*wavSize)) {
// //     ESP_LOGI(TAG, "Failed to write all data to file.\n");
// //   } else {
// //     ESP_LOGI(TAG, "File written successfully.\n");
// //   }

// //   // Close the file
// //   file.close();
//   return wavFile;
//   }
//   else return nullptr;
// }



// void selectNumOfBands(JsonDocument& dCommand){
//   uint8_t cmd = dCommand["app_command"];
//   audioSpecVisual_Set.noOfBands = dCommand["numOfBands"];
//   mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));        
//   mqtt_Command_Respond_Success(cmd, pdPASS);
// }

// void selectPattern(JsonDocument& dCommand){
//   uint8_t cmd = dCommand["app_command"];
//   audioSpecVisual_Set.selectedPattern = dCommand["selectedPattern"];
//   mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));        
//   mqtt_Command_Respond_Success(cmd, pdPASS);
// }

// void setRandomPatterns(JsonDocument& dCommand){
//   uint8_t cmd = dCommand["app_command"];
//   audioSpecVisual_Set.showRandom = dCommand["showRandom"];
//   if(audioSpecVisual_Set.showRandom) xTimerStart(showRandomPatternTimer_H, 0);
//   else xTimerStop(showRandomPatternTimer_H, 0);
//   mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));        
//   mqtt_Command_Respond_Success(cmd, pdPASS);
// }

// void setSensitivity(JsonDocument& dCommand){
//   uint8_t cmd = dCommand["app_command"];
//   audioSpecVisual_Set.sensitivity = dCommand["sensitivity"];
//   mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));        
//   mqtt_Command_Respond_Success(cmd, pdPASS);
// }
