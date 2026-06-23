
/**
 * @file mtb_audio.cpp
 * @brief Implementation of the PIXLPAL-M1 audio subsystem.
 *
 * Implements the MTB_Audio playback class, dual-pipeline FreeRTOS tasks for I2S DAC
 * output (audio_Out_Processing_Task) and I2S microphone capture (audio_In_Processing_Task),
 * USB Audio Class (UAC) speaker support via the ESP-IDF uac_host driver, the arduinoFFT
 * spectrum visualiser loop, and all init / de-init helpers declared in mtb_audio.h.
 */
#include "stdlib.h"
#include "stdio.h"
#include "arduinoFFT.h"
#include <math.h>
#include <cstdint>
#include "esp_system.h"
#include <time.h>
#include "FFT.h"
#include "mtb_nvs.h"
#include "ledDriver.h"
#include "patternsHUB75.h"
#include "mtb_audio.h"
#include "mtb_engine.h"
#include "mtb_usb_fs.h"   // make sure this is in your include path
#include "microphone.h"
#include "my_secret_keys.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
static const char TAG[] = "METERBIT_DAC_N_MIC";
#include "usb/usb_host.h"
#include "usb/uac_host.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
//static const char *TAG = "UAC_PIPE";

// ---- Audio format (match what you opened the speaker with) ----
#define USB_SRATE_HZ            48000
#define USB_BITS_PER_SAMPLE     16
#define USB_CHANNELS            2

#define UAC_TASK_PRIORITY       5
#define DEFAULT_UAC_FREQ        48000
#define DEFAULT_UAC_BITS        16
#define DEFAULT_UAC_CH          2

// Derived sizes
#define BYTES_PER_FRAME   (USB_CHANNELS * (USB_BITS_PER_SAMPLE/8))   // 4
#define BYTES_PER_MS      ((USB_SRATE_HZ/1000) * BYTES_PER_FRAME)    // 192

// How many 1ms packets per USB transfer we push at once
#ifndef PACKETS_PER_XFER
#define PACKETS_PER_XFER  4                                          // 4ms => 768B
#endif
#define XFER_BYTES        (BYTES_PER_MS * PACKETS_PER_XFER)           // 768

// Ringbuffer capacity: a few hundred ms of audio to absorb jitter
#define PCM_RB_CAPACITY   (BYTES_PER_MS * 400)                        // ~400ms

EXT_RAM_BSS_ATTR StaticQueue_t xQueueStorage_AudioTextInfo;
EXT_RAM_BSS_ATTR RingbufHandle_t s_pcm_rb = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t s_uac_writer_task = NULL;
static volatile bool s_uac_running = false;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
EXT_RAM_BSS_ATTR QueueHandle_t s_event_queue = NULL;
EXT_RAM_BSS_ATTR uac_host_device_handle_t s_spk_dev_handle = NULL;

static uint32_t s_spk_curr_freq = DEFAULT_UAC_FREQ;
static uint8_t s_spk_curr_bits = DEFAULT_UAC_BITS;
static uint8_t s_spk_curr_ch = DEFAULT_UAC_CH;
static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg);
static void uac_host_lib_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg);
static void uac_Speaker_Task(void *d_Service);
static void usb_Speaker_Process_Task(void *d_Service);
static void start_uac_pipeline(void);
static void stop_uac_pipeline(void);
static void uac_writer_task(void *arg);

/**
 * @brief event group
 *
 * APP_EVENT            - General control event
 * UAC_DRIVER_EVENT     - UAC Host Driver event, such as device connection
 * UAC_DEVICE_EVENT     - UAC Host Device event, such as rx/tx completion, device disconnection
 */
typedef enum {
    APP_EVENT = 0,
    UAC_DRIVER_EVENT,
    UAC_DEVICE_EVENT,
} event_group_t;

/**
 * @brief event queue
 *
 * This event is used for delivering the UAC Host event from callback to the uac_lib_task
 */
typedef struct {
    event_group_t event_group;
    union {
        struct {
            uint8_t addr;
            uint8_t iface_num;
            uac_host_driver_event_t event;
            void *arg;
        } driver_evt;
        struct {
            uac_host_device_handle_t handle;
            uac_host_device_event_t event;
            void *arg;
        } device_evt;
    };
} s_event_queue_t;
// 
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
EXT_RAM_BSS_ATTR TimerHandle_t showRandomPatternTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t audioOutProcessing_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t audioInProcessing_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t usb_Speaker_Process_Task_H = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t audioTextInfo_Q = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t dac_Start_Sem_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t mic_Start_Sem_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t audio_In_Data_Collected_Sem_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t uac_task_handle = NULL;

AudioSpectVisual_Set_t audioSpecVisual_Set{
    .selectedPattern = 0,
    .noOfBands = 32,
    .showRandom = 0,
    .randomInterval = 4
};

bool completedAudioMicConfig = false;
AUDIO_I2S_MODE_T audioOutMode = CONNECT_HOST;
Audio *audio = nullptr;
MTB_Audio *mtb_audioPlayer = nullptr;
uint8_t mic_OR_dac = OFF_DAC_N_MIC;

RawAudioData AudioSamplesTransport;
//AUDIO_TEXT_DATA_T selectAudioText = AUDIO_INFO;
EXT_RAM_BSS_ATTR AudioTextTransfer_T audioTextTransferBuffer;

//uint8_t VisualizeAudio = false;
uint8_t kMatrixWidth = 128; //   PANEL_WIDTH
uint8_t kMatrixHeight = 64; // PANEL_HEIGHT
int loopcounter = 0;

EXT_RAM_BSS_ATTR Mtb_Services *UAC_Speaker_Sv = new Mtb_Services(uac_Speaker_Task, &uac_task_handle, "uac_events", 4096, 2, 0);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Usb_Audio_Sv = new Mtb_Services(usb_Speaker_Process_Task, &usb_Speaker_Process_Task_H, "Usb Aud Serv.", 4096, 5, 0);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Audio_Out_Sv = new Mtb_Services(audio_Out_Processing_Task, &audioOutProcessing_Task_H, "Aud Out Serv.", 6144, 2, 1);
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Audio_In_Sv = new Mtb_Services(audio_In_Processing_Task, &audioInProcessing_Task_H, "Aud In Serv.", 4096, 2, 1);

void audio_Out_Processing_Task(void *d_Service){
  Mtb_Services *thisServ = (Mtb_Services *)d_Service;
  mtb_audioPlayer = new MTB_Audio();
  Audio::audio_info_callback = mtb_Audio_Info;
  init_Audio_Out_Processing();
  uint8_t *audioTextInfoQueue_buffer = (uint8_t *)heap_caps_malloc(6 * sizeof(AudioTextTransfer_T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(audioTextInfo_Q == NULL) audioTextInfo_Q = xQueueCreateStatic(6, sizeof(AudioTextTransfer_T), audioTextInfoQueue_buffer, &xQueueStorage_AudioTextInfo);
  mtb_Read_Nvs_Struct("dev_Volume", &deviceVolume, sizeof(uint8_t));

  audio = new Audio();
  delay(100);                                    // wait for the audio object to be successfully created.
  audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio->setVolume(deviceVolume);
  audio->setConnectionTimeout(65000, 65000);

//######################################################################################################################
  //mtb_Launch_This_Service(mtb_Usb_Audio_Sv);
//######################################################################################################################

  while (MTB_SERV_IS_ACTIVE == pdTRUE){
    if(xSemaphoreTake(dac_Start_Sem_H, pdMS_TO_TICKS(25)) == pdTRUE){

    switch(audioOutMode){
      case CONNECT_HOST: mtb_audioPlayer->contdSucceed = (int8_t) audio->connecttohost(mtb_audioPlayer->host_Url.c_str(), mtb_audioPlayer->host_Username.c_str(), mtb_audioPlayer->host_Password.c_str());
        break;
      case OPENAI_SPEECH: mtb_audioPlayer->contdSucceed = (int8_t) audio->openai_speech(String(openai_key), mtb_audioPlayer->openAI_Model, mtb_audioPlayer->speech_Message, mtb_audioPlayer->openAI_Instructions, mtb_audioPlayer->openAI_Voice, mtb_audioPlayer->openAI_ResponseFormat, mtb_audioPlayer->openAI_Speed);
        break;
      case CONNECT_SPEECH: mtb_audioPlayer->contdSucceed = (int8_t) audio->connecttospeech(mtb_audioPlayer->speech_Message.c_str(), mtb_audioPlayer->ggle_Lang.c_str());
        break;
      case CONNECT_USB_FS: mtb_audioPlayer->contdSucceed = (int8_t) audio->connecttoFS(USBFS, mtb_audioPlayer->filePath.c_str(), mtb_audioPlayer->fileStartPos);
        break;
      default: ESP_LOGE("MTB_AUDIO", "Audio Output Mode Not Specified.");
      goto KillAudio;
      break;
    }

    while(mic_OR_dac == ON_DAC && MTB_SERV_IS_ACTIVE == pdTRUE){
      audio->loop();
      vTaskDelay(1);
    }// loop end
      audio->stopSong();
  }
  //******* DAC CODE LOOP ENDS HERE *********************************************************************************************************** */
}

  KillAudio:
    delay(100);
    delete audio;
    audio = nullptr;

//######################################################################################################################
  //mtb_Delete_This_Service(mtb_Usb_Audio_Sv);
//###################################################################################################################### 
  delete mtb_audioPlayer;
  vQueueDelete(audioTextInfo_Q); audioTextInfo_Q = NULL; //The Queue remains active for the next audio service. This prevents apps such as the Internet Radio from crashing.
  de_init_Audio_Out_Processing();
  mtb_Delete_This_Service(thisServ);
}

void audio_In_Processing_Task(void *d_Service){
  Mtb_Services *thisServ = (Mtb_Services *)d_Service;
  init_Audio_In_Processing();

    // Array to store Original audio I2S input stream (reading in chunks, e.g. 1024 values) 
    int16_t audio_buffer[1024];   // 1024 values [2048 bytes] <- for the original I2S signed 16bit stream 
    // now reading the I2S input stream (with NEW <I2S_std.h>)
    size_t bytes_read = 0;
//######################################################################################################################
    I2S_Record_Init();

  while (MTB_SERV_IS_ACTIVE == pdTRUE){

  //****** MIC CODE LOOP STARTS HERE ************************************************************************************************************ */  
  //ESP_LOGI(TAG, "Microphone service is launched and waiting for activation.\n");
  if(xSemaphoreTake(mic_Start_Sem_H, pdMS_TO_TICKS(25)) == pdTRUE){

      while (mic_OR_dac == ON_MIC && MTB_SERV_IS_ACTIVE == pdTRUE){
        if(i2s_channel_read(rx_handle, audio_buffer, sizeof(audio_buffer), &bytes_read, pdMS_TO_TICKS(1000)) != ESP_OK) continue;
        // Optionally: Boostering the very low I2S Microphone INMP44 amplitude (multiplying values with factor GAIN_BOOSTER_I2S)
        for (int16_t i = 0; i < (bytes_read / 2); ++i) AudioSamplesTransport.audioBuffer[i] = audio_buffer[i] * GAIN_BOOSTER_I2S;
        AudioSamplesTransport.audioSampleLength_bytes = bytes_read;
        xSemaphoreGive(audio_In_Data_Collected_Sem_H);
    }

  }
  //****** MIC CODE LOOP ENDS HERE ************************************************************************************************************ */
  }
  I2S_Record_De_Init();
  de_init_Audio_In_Processing();
  mtb_Delete_This_Service(thisServ);
}

void init_Audio_Out_Processing(void){
  if(dac_Start_Sem_H == NULL) dac_Start_Sem_H = xSemaphoreCreateBinary();
  //if(usbSpk_Ready_Sem_H == NULL) usbSpk_Ready_Sem_H = xSemaphoreCreateBinary(); // To reuse USB Speaker, uncomment this line and get library from Archive @ 23 Jan 2025
}

void init_Audio_In_Processing(void){
  if(mic_Start_Sem_H == NULL) mic_Start_Sem_H = xSemaphoreCreateBinary();
  if(audio_In_Data_Collected_Sem_H == NULL) audio_In_Data_Collected_Sem_H = xSemaphoreCreateBinary();  
//************************************************************************************************************************************ */
  if(AudioSamplesTransport.audioBuffer == nullptr) AudioSamplesTransport.audioBuffer = (int16_t*) calloc(SAMPLEBLOCK, sizeof(int16_t));
}

void de_init_Audio_Out_Processing(void){
  vSemaphoreDelete(dac_Start_Sem_H); dac_Start_Sem_H = NULL;
}

void de_init_Audio_In_Processing(void){
  if(AudioSamplesTransport.audioBuffer != nullptr) {
    free(AudioSamplesTransport.audioBuffer); 
    AudioSamplesTransport.audioBuffer = nullptr;
  }

  vSemaphoreDelete(mic_Start_Sem_H); mic_Start_Sem_H = NULL;
  if(audio_In_Data_Collected_Sem_H != NULL){
    vSemaphoreDelete(audio_In_Data_Collected_Sem_H); audio_In_Data_Collected_Sem_H = NULL;
  } 
}

bool I2S_Record_Init() {  
  // Get the default channel configuration by helper macro (defined in 'i2s_common.h')
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);     // Allocate a new RX channel and get the handle of this channel
  i2s_channel_init_std_mode(rx_handle, &std_cfg);   // Initialize the channel
  i2s_channel_enable(rx_handle);                    // Before reading data, start the RX channel first

  /* Not used: 
  i2s_channel_disable(rx_handle);                   // Stopping the channel before deleting it 
  i2s_del_channel(rx_handle);                       // delete handle to release the channel resources */
  
  flg_I2S_initialized = true;                       // all is initialized, checked in procedure Record_Start()

  ESP_LOGI(TAG, "Microphone I2S channel enabled and created.\n");

  return flg_I2S_initialized;  
}

bool I2S_Record_De_Init(){
  i2s_channel_disable(rx_handle);
  i2s_del_channel(rx_handle);
  ESP_LOGI(TAG, "Microphone I2S channel disabled and deleted.\n");
  return true;  
}

void initAudioVisualPattern(void){
  if(vReal == nullptr) vReal = (double*)calloc(SAMPLEBLOCK, sizeof(double));
  if(vImag == nullptr) vImag = (double*)calloc(SAMPLEBLOCK, sizeof(double));
  if(FreqBins == nullptr) FreqBins = (float*)calloc(65, sizeof(float));
  if(oldBarHeights == nullptr) oldBarHeights = (int*)calloc(65, sizeof(int));
  if(peak == nullptr) peak = (uint8_t*)calloc(65, sizeof(uint8_t));
  if(PeakFlag == nullptr) PeakFlag = (char*)calloc(64, sizeof(char));
  if(PeakTimer == nullptr) PeakTimer = (int*)calloc(64, sizeof(int));

  srand(time(NULL));
  mtb_Read_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
  if(showRandomPatternTimer_H == NULL) showRandomPatternTimer_H = xTimerCreate("rand pat tim", pdMS_TO_TICKS(audioSpecVisual_Set.randomInterval * 1000), pdTRUE, NULL, randomPattern_TimerCallback);
}

void deInitAudioVisualPattern(void){
  if(vReal != nullptr) free(vReal); 
  vReal = nullptr;
  if(vImag != nullptr) free(vImag); 
  vImag = nullptr;
  if(FreqBins != nullptr) free(FreqBins); 
  FreqBins = nullptr;
  if(oldBarHeights != nullptr) free(oldBarHeights); 
  oldBarHeights = nullptr;
  if(peak != nullptr) free(peak); 
  peak = nullptr;
  if(PeakFlag != nullptr) free(PeakFlag); 
  PeakFlag = nullptr;
  if(PeakTimer != nullptr) free(PeakTimer); 
  PeakTimer = nullptr;
  
  // Stop the timer before deleting
  if(showRandomPatternTimer_H != NULL){
    if(xTimerIsTimerActive(showRandomPatternTimer_H) != pdFALSE) xTimerStop(showRandomPatternTimer_H, 0);
    xTimerDelete(showRandomPatternTimer_H, 0);
    showRandomPatternTimer_H = NULL;
  }
}
//######################################################################################
// BucketFrequency
//
// Return the frequency corresponding to the Nth sample bucket.  Skips the first two 
// buckets which are overall amplitude and something else.
int BucketFrequency(int iBucket){
 if (iBucket <= 1)return 0;
 int iOffset = iBucket - 2;
 return iOffset * (samplingFrequency / 2) / (SAMPLEBLOCK / 2);
}

void mtb_Dac_Or_Mic_Status(uint8_t mtb_i2s_Module){
  mic_OR_dac = mtb_i2s_Module;
  delay(100);
  switch(mic_OR_dac){
    case ON_DAC:xSemaphoreGive(dac_Start_Sem_H);
      break;
    case ON_MIC: xSemaphoreGive(mic_Start_Sem_H);
      break;
    default:  ESP_LOGI(TAG, "Mic and/or Speakers Deactivated. \n");
      break;
    }
}


void mtb_Audio_Info(Audio::msg_t m) {
    switch(m.e){
        case Audio::evt_info:           
          ESP_LOGI(TAG, "AudioI2S Info: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_INFO;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break;
        case Audio::evt_eof:            
          ESP_LOGI(TAG, "EOF MP3: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_EOF_MP3;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break;
        case Audio::evt_bitrate:        
          ESP_LOGI(TAG, "Bitrate: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_BITRATE;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0); 
        break; // icy-bitrate or bitrate from metadata
        case Audio::evt_icyurl:         
          ESP_LOGI(TAG, "ICYURL: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_ICYURL;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break;
        case Audio::evt_id3data:        
          ESP_LOGI(TAG, "ID3 Data: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_ID3DATA;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break; // id3-data or metadata
        case Audio::evt_lasthost:       
          ESP_LOGI(TAG, "LastHost: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_LASTHOST;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break;
        case Audio::evt_name:           
          ESP_LOGI(TAG, "Station: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_SHOW_STATION;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break; // station name or icy-name
        case Audio::evt_streamtitle:    
          ESP_LOGI(TAG, "Stream Title: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_SHOWS_STREAM_TITLE;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
        break;
        case Audio::evt_icylogo:
          ESP_LOGI(TAG, "ICY Logo: ... %s\n", m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_ICYLOGO;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
        break;
        case Audio::evt_icydescription:
          ESP_LOGI(TAG, "ICY Description: %s \n",  m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_ICY_DESCRIPTION;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0);
         break;

        case Audio::evt_image: for(int i = 0; i < m.vec.size(); i += 2){ Serial.printf("cover image:  segment %02i, pos %07lu, len %05lu\n", i / 2, m.vec[i], m.vec[i + 1]);} break; // APIC

        case Audio::evt_lyrics:         
          ESP_LOGI(TAG, "Sync Lyrics:  %s\n", m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_SYNC_LYRICS;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0); 
        break;
        case Audio::evt_log:
          ESP_LOGI(TAG, "Audio Logs:   %s\n", m.msg);
          audioTextTransferBuffer.Audio_Text_type = AUDIO_LOG;
          strcpy(audioTextTransferBuffer.Audio_Text_Data, m.msg);
          xQueueSend(audioTextInfo_Q, &audioTextTransferBuffer, 0); 
        break;
        default:    ESP_LOGI(TAG, "Message:..... %s\n", m.msg); break;
    }
}

// void audio_process_i2s(int16_t* outBuff, uint16_t validSamples, bool *continueI2S)
// {
//     // Compute bytes in this callback block (treat validSamples as *frames*)
//     const size_t bytes_per_frame = 4; // 2 channels * 2 bytes per sample. 
//     size_t len_bytes = (size_t)validSamples * bytes_per_frame;

//     // Non-blocking push to the ringbuffer; if full, we drop (or you can overwrite)
//     if (s_pcm_rb) {
//         BaseType_t ok = xRingbufferSend(s_pcm_rb, outBuff, len_bytes, 0);
//         (void)ok; // optional: log drop if !ok
//     }

//     // If you only want USB output, set false. If you also want I2S DAC, keep true.
//     if (continueI2S) *continueI2S = false;
// }



void audioVisualizer(){
    AudioSamplesTransport.audioSampleLength_bytes /= 2;
    int PeakDirection = 0;
    // ############ Step 2: compensate for Channel number and offset, safe all to vReal Array
    for (uint16_t i = 0; i < AudioSamplesTransport.audioSampleLength_bytes; i++){
      vReal[i] = AudioSamplesTransport.audioBuffer[i];
//      *audioSpecVisual_Set.sensitivity;
    }
    memset(vImag, 0, SAMPLEBLOCK * sizeof(double)); // Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
    // ############ Step 3: Do FFT on the VReal array  ############
    // compute FFT
    FFT->DCRemoval(vReal, SAMPLEBLOCK);
    FFT->Windowing(vReal, SAMPLEBLOCK, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT->Compute(vReal, vImag, SAMPLEBLOCK, FFT_FORWARD);
    FFT->ComplexToMagnitude(vReal, vImag, SAMPLEBLOCK);
    FFT->MajorPeak(vReal, SAMPLEBLOCK, samplingFrequency);

    for (int i = 0; i < audioSpecVisual_Set.noOfBands; i++)
    {
      FreqBins[i] = 0;
    }
    // ############ Step 4: Fill the frequency bins with the FFT Samples ############
    float averageSum = 0.0f;
    for (int i = 2; i < SAMPLEBLOCK / 2; i++){
      averageSum += vReal[i];
      if (vReal[i] > NoiseTresshold){
        int freq = BucketFrequency(i);
        int iBand = 0;
        while (iBand < audioSpecVisual_Set.noOfBands)
        {
          if (freq < BandCutoffTable[iBand])
            break;
          iBand++;
        }
        if (iBand > audioSpecVisual_Set.noOfBands)
          iBand = audioSpecVisual_Set.noOfBands;
        FreqBins[iBand] += vReal[i];
        //  float scaledValue = vReal[i];
        //  if (scaledValue > peak[iBand])
        //    peak[iBand] = scaledValue;
      }
    }

    // bufmd[0]=FreqBins[12];

    //*
    // ############ Step 6: Averaging and making it all fit on screen
    // for (int i = 0; i < numBands; i++) {
    // Serial.printf ("Chan[%d]:%d",i,(int)FreqBins[i]);
    // FreqBins[i] = powf(FreqBins[i], gLogScale); // in case we want log scale..i leave it in here as reminder
    //  ESP_LOGI(TAG,  " - log: %d \n",(int)FreqBins[i]);
    // }
    static float lastAllBandsPeak = 0.0f;
    float allBandsPeak = 0;
    // bufmd[1]=FreqBins[13];
    // bufmd[2]=FreqBins[1];
    for (int i = 0; i < audioSpecVisual_Set.noOfBands; i++)
    {
      // allBandsPeak = max (allBandsPeak, FreqBins[i]);
      if (FreqBins[i] > allBandsPeak)
      {
        allBandsPeak = FreqBins[i];
      }
    }
    if (allBandsPeak < 1)
      allBandsPeak = 1;
    //  The followinf picks allBandsPeak if it's gone up.  If it's gone down, it "averages" it by faking a running average of GAIN_DAMPEN past peaks
    allBandsPeak = max(allBandsPeak, ((lastAllBandsPeak * (GAIN_DAMPEN - 1)) + allBandsPeak) / GAIN_DAMPEN); // Dampen rate of change a little bit on way down
    lastAllBandsPeak = allBandsPeak;

    if (allBandsPeak < 80000)
      allBandsPeak = 80000;
    for (int i = 0; i < audioSpecVisual_Set.noOfBands; i++)
    {
      FreqBins[i] /= (allBandsPeak * 1.0f);
    }

    // Process the FFT data into bar heights
    for (int band = 0; band < audioSpecVisual_Set.noOfBands; band++)
    {
      int barHeight = FreqBins[band] * kMatrixHeight - 1; //(AMPLITUDE);
      if (barHeight > TOP - 2)
        barHeight = TOP - 2;

      // Small amount of averaging between frames
      barHeight = ((oldBarHeights[band] * 1) + barHeight) / 2;

      // Move peak up
      if (barHeight > peak[band])
      {
        peak[band] = min(TOP, barHeight);
        PeakFlag[band] = 1;
      }

#if BottomRowAlwaysOn
       if(barHeight == 0) barHeight = 1; // make sure there is always one bar that lights up
#endif

    switch(audioSpecVisual_Set.selectedPattern){
       // Now visualize those bar heights
       case PATTERN_0 :
        PeakDirection = AUD_VIS_DOWN;
        BoxedBars(band, barHeight);
        BluePeak(band);
           break;

       case PATTERN_1 :
        PeakDirection = AUD_VIS_DOWN;
        BoxedBars2(band, barHeight);
        BluePeak(band);
           break;

       case PATTERN_2 :
         PeakDirection = AUD_VIS_DOWN;
         BoxedBars3(band, barHeight);
         RedPeak(band);
           break;

       case PATTERN_3 :
         PeakDirection = AUD_VIS_DOWN;
         RedBars(band, barHeight);
         BluePeak(band);
           break;

       case PATTERN_4 :
         PeakDirection = AUD_VIS_DOWN;
         ColorBars(band, barHeight);
           break;

       case PATTERN_5 :
         PeakDirection = AUD_VIS_DOWN;
         Twins(band, barHeight);
         WhitePeak(band);
           break;

       case PATTERN_6 :
         PeakDirection = AUD_VIS_DOWN;
         Twins2(band, barHeight);
         WhitePeak(band);
           break;

       case PATTERN_7 :
          PeakDirection = AUD_VIS_DOWN;
         TriBars(band, barHeight);
         DoublePeak(band);
           break;

       case PATTERN_8 :
         PeakDirection = AUD_VIS_DOWN;
         TriBars(band, barHeight);
         TriPeak(band);
           break;

       case PATTERN_9 :
        PeakDirection = AUD_VIS_DOWN;
        centerBars(band, barHeight);
           break;

       case PATTERN_10 :
        PeakDirection = AUD_VIS_DOWN;
        centerBars2(band, barHeight);
           break;

       case PATTERN_11 :
       PeakDirection = AUD_VIS_DOWN;
       BlackBars(band, barHeight);
       DoublePeak(band);
           break;
           
        case PATTERN_12 :
        GradientBars(band, barHeight);
        break;
        case PATTERN_13 :
        CheckerboardBars(band, barHeight);
        break;
        case PATTERN_14 :
        RainbowGradientBars(band, barHeight);
        break;
        case PATTERN_15 :
        StripedBars(band, barHeight);
        break;
        case PATTERN_16 :
        DiagonalBars(band, barHeight);
        break;
        case PATTERN_17 :
        VerticalGradientBars(band, barHeight);
        break;
        case PATTERN_18 :
        ZigzagBars(band, barHeight);
        break;
        case PATTERN_19 :
        DottedBars(band, barHeight);
        break;
        case PATTERN_20 :
        ColorFadeBars(band, barHeight);
        break;
        case PATTERN_21 :
        PulsingBars(band, barHeight);
        break;
        case PATTERN_22 :
        FlashingBars(band, barHeight);
        break;
        case PATTERN_23 :
         PeakDirection = AUD_VIS_UP;
         TriBars(band, barHeight);
         TriPeak(band);
           break;
        default:
          break;
      }
       // Save oldBarHeights for averaging later
       oldBarHeights[band] = barHeight;
     }
     // for calibration
     if (loopcounter == 256){loopcounter = 0;}

 loopcounter++;
   // Decay peak
//  EVERY_N_MILLISECONDS(Fallingspeed){
//    for (uint8_t band = 0; band < audioSpecVisual_Set.noOfBands; band++){
//      if(PeakFlag[band]==1){
//        PeakTimer[band]++;
//        if (PeakTimer[band]> Peakdelay){PeakTimer[band]=0;PeakFlag[band]=0;}
//      }
//      else if ((peak[band] > 0) &&(PeakDirection==AUD_VIS_UP)){ 
//        peak[band] += 1;
//        if (peak[band]>(kMatrixHeight+10))peak[band]=0;
//        } // when to far off screen then reset peak height
//      else if ((peak[band] > 0)&&(PeakDirection==AUD_VIS_DOWN)){ peak[band] -= 1;}
//    }   
//  }
}

 void randomPattern_TimerCallback(TimerHandle_t dHandle){
  audioSpecVisual_Set.selectedPattern = rand() % NO_OF_AUDSPEC_PATTERNS;
 }

bool MTB_Audio::mtb_Openai_Speech(const String& model, const String& input, const String& instructions, const String& voice, const String& response_format, const String& speed){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;
  
  openAI_Model = model;
  speech_Message = input;
  openAI_Instructions = instructions;
  openAI_Voice = voice;
  openAI_ResponseFormat = response_format;
  openAI_Speed = speed;
  audioOutMode = OPENAI_SPEECH;

  mtb_Dac_Or_Mic_Status(ON_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}

bool MTB_Audio::mtb_ConnectToHost(const char* host, const char* user, const char* pwd){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1; 
  
  host_Url = String(host);
  host_Username = String(user);
  host_Password = String(pwd);

  audioOutMode = CONNECT_HOST;
  mtb_Dac_Or_Mic_Status(ON_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}

bool MTB_Audio::mtb_ConnectToSpeech(const char* speech, const char* lang){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;

  speech_Message = String(speech);
  ggle_Lang = String(lang);

  audioOutMode = CONNECT_SPEECH;
  mtb_Dac_Or_Mic_Status(ON_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}

bool MTB_Audio::mtb_ConnectToUSB_FS( const char *path, int32_t m_fileStartPos){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;
  
  filePath = String(path);
  fileStartPos = m_fileStartPos;

  bool isMounted = USBFS.begin("/usb");
  if (isMounted){
        audioOutMode = CONNECT_USB_FS;
        mtb_Dac_Or_Mic_Status(ON_DAC);
  }
  else {
    ESP_LOGE("MTB_AUDIO", "USBFS not mounted. Cannot connect to USB_FS.");
    return false;
  }

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}




// static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
// {
//     if (s_spk_dev_handle == NULL) {
//         return ESP_ERR_INVALID_STATE;
//     }
//     *bytes_written = 0;
//     esp_err_t ret = uac_host_device_write(s_spk_dev_handle, audio_buffer, len, timeout_ms);
//     if (ret == ESP_OK) {
//         *bytes_written = len;
//     }
//     return ret;
// }


void usb_Speaker_Process_Task(void *d_Service){
  Mtb_Services *thisServ = (Mtb_Services *)d_Service;

    s_event_queue = xQueueCreate(10, sizeof(s_event_queue_t)); // REVISIT -> Potential memory savings by putting queue in PSRAM. Or delete after use.
    assert(s_event_queue != NULL);

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installed");

    mtb_Launch_This_Service(UAC_Speaker_Sv);
    delay(100);
    xTaskNotifyGive(uac_task_handle);

    while (MTB_SERV_IS_ACTIVE == pdTRUE) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        // In this example, there is only one client registered
        // So, once we deregister the client, this call must succeed with ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    mtb_Delete_This_Service(UAC_Speaker_Sv);

    ESP_LOGI(TAG, "USB Host shutdown");
    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());

mtb_Delete_This_Service(thisServ);

}

static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg){
    if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED) {
        // stop audio player first
        s_spk_dev_handle = NULL;
        //audio_player_stop();
        ESP_LOGI(TAG, "UAC Device disconnected");
        ESP_ERROR_CHECK(uac_host_device_close(uac_device_handle));
        return;
    }
    // Send uac device event to the event queue
    s_event_queue_t evt_queue = {};
        evt_queue.event_group = UAC_DEVICE_EVENT;
        evt_queue.device_evt.handle = uac_device_handle;
        evt_queue.device_evt.event = event;
        evt_queue.device_evt.arg = arg;
    // should not block here
    xQueueSend(s_event_queue, &evt_queue, 0);
}

static void uac_host_lib_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg){
    // Send uac driver event to the event queue
    s_event_queue_t evt_queue = {};
        evt_queue.event_group = UAC_DRIVER_EVENT;
        evt_queue.driver_evt.addr = addr;
        evt_queue.driver_evt.iface_num = iface_num;
        evt_queue.driver_evt.event = event;
        evt_queue.driver_evt.arg = arg;
        xQueueSend(s_event_queue, &evt_queue, 0);
}

void uac_Speaker_Task(void *d_Service){
    Mtb_Services *thisServ = (Mtb_Services *)d_Service;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);                              // REVISIT -> This task seems to wait indefinitely. Confirm that the service does closes when required.
    uac_host_driver_config_t uac_config = {
        .create_background_task = true,
        .task_priority = UAC_TASK_PRIORITY,
        .stack_size = 4096,
        .core_id = 0,
        .callback = uac_host_lib_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(uac_host_install(&uac_config));
    ESP_LOGI(TAG, "UAC Class Driver installed");
    s_event_queue_t evt_queue = { .event_group = (event_group_t)0 };
        while (MTB_SERV_IS_ACTIVE == pdTRUE) {
        if (xQueueReceive(s_event_queue, &evt_queue, portMAX_DELAY)) {      // REVISIT -> This task seems to wait indefinitely. Confirm that the service does closes when required.
            if (UAC_DRIVER_EVENT ==  evt_queue.event_group) {
                uac_host_driver_event_t event = evt_queue.driver_evt.event;
                uint8_t addr = evt_queue.driver_evt.addr;
                uint8_t iface_num = evt_queue.driver_evt.iface_num;
                switch (event) {
                case UAC_HOST_DRIVER_EVENT_TX_CONNECTED: {
                    uac_host_dev_info_t dev_info;
                    uac_host_device_handle_t uac_device_handle = NULL;
                    const uac_host_device_config_t dev_config = {
                        .addr = addr,
                        .iface_num = iface_num,
                        .buffer_size = 16000,
                        .buffer_threshold = 4000,
                        .callback = uac_device_callback,
                        .callback_arg = NULL,
                    };
                    ESP_ERROR_CHECK(uac_host_device_open(&dev_config, &uac_device_handle));
                    ESP_ERROR_CHECK(uac_host_get_device_info(uac_device_handle, &dev_info));
                    ESP_LOGI(TAG, "UAC Device connected: SPK");
                    uac_host_printf_device_param(uac_device_handle);
                    // Start usb speaker with the default configuration
                    const uac_host_stream_config_t stm_config = {
                        .channels = s_spk_curr_ch,
                        .bit_resolution = s_spk_curr_bits,
                        .sample_freq = s_spk_curr_freq,
                    };
                    ESP_ERROR_CHECK(uac_host_device_start(uac_device_handle, &stm_config));
                    s_spk_dev_handle = uac_device_handle;

                    start_uac_pipeline();
                    break;
                }
                case UAC_HOST_DRIVER_EVENT_RX_CONNECTED: {
                    // we don't support MIC in this example
                    ESP_LOGI(TAG, "UAC Device connected: MIC");
                    break;
                }
                default:
                    break;
                }
            } else if (UAC_DEVICE_EVENT == evt_queue.event_group) {
                uac_host_device_event_t event = evt_queue.device_evt.event;
                switch (event) {
                case UAC_HOST_DRIVER_EVENT_DISCONNECTED:
                    s_spk_curr_bits = DEFAULT_UAC_BITS;
                    s_spk_curr_freq = DEFAULT_UAC_FREQ;
                    s_spk_curr_ch = DEFAULT_UAC_CH;
                    ESP_LOGI(TAG, "UAC Device disconnected");
                    stop_uac_pipeline();
                    break;
                case UAC_HOST_DEVICE_EVENT_RX_DONE:
                    break;
                case UAC_HOST_DEVICE_EVENT_TX_DONE:
                    break;
                case UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR:
                    break;
                default:
                    break;
                }
            } else if (APP_EVENT == evt_queue.event_group) {
                break;
            }
        }
    }

    ESP_LOGI(TAG, "UAC Driver uninstall");
    ESP_ERROR_CHECK(uac_host_uninstall());
    delay(50);  // allow uac memory cleanup

    mtb_Delete_This_Service(thisServ);
}


static void uac_writer_task(void *arg)
{
    // Small staging buffer to ensure exact xfer sizes
    uint8_t *acc = (uint8_t*)heap_caps_malloc(XFER_BYTES, MALLOC_CAP_8BIT);
    size_t   acc_len = 0;

    // Prebuilt 768-byte silence block for underruns
    static uint8_t silence[XFER_BYTES] = {0};

    const TickType_t write_timeout = pdMS_TO_TICKS(10);
    s_uac_running = true;

    while (s_uac_running) {
        // 1) Top up the accumulator from ringbuffer
        if (acc_len < XFER_BYTES) {
            size_t got = 0;
            uint8_t *blk = NULL;

            // Pull up to what we need to fill a xfer (non-busy wait)
            blk = (uint8_t*)xRingbufferReceiveUpTo(
                    s_pcm_rb, &got, pdMS_TO_TICKS(20), XFER_BYTES - acc_len);

            if (blk && got) {
                memcpy(acc + acc_len, blk, got);
                acc_len += got;
                vRingbufferReturnItem(s_pcm_rb, blk);
            }
        }

        // 2) If not enough data, send silence to keep steady isoch cadence
        if (acc_len < XFER_BYTES) {
            if (s_spk_dev_handle) {
                esp_err_t e = uac_host_device_write(s_spk_dev_handle, silence, XFER_BYTES, write_timeout);
                (void)e; // optional: log if e != ESP_OK
            }
            continue; // try again to fill acc
        }

        // 3) We have a full XFER_BYTES chunk — write it
        if (s_spk_dev_handle) {
            esp_err_t e = uac_host_device_write(s_spk_dev_handle, acc, XFER_BYTES, write_timeout);
            if (e != ESP_OK) {
                // If this happens repeatedly, consider backing off or resetting acc_len
                // ESP_LOGW(TAG, "uac write failed: %d", e);
            }
        }

        // 4) Reset accumulator
        acc_len = 0;
    }

    free(acc);
    vTaskDelete(NULL);
}


static void start_uac_pipeline(void)
{
    if (!s_pcm_rb) {
        s_pcm_rb = xRingbufferCreate(PCM_RB_CAPACITY, RINGBUF_TYPE_BYTEBUF);
    }
    if (!s_uac_writer_task) {
        xTaskCreatePinnedToCore(uac_writer_task, "uac_wr", 4096, NULL, 5, &s_uac_writer_task, tskNO_AFFINITY);
    }
}

static void stop_uac_pipeline(void)
{
    s_uac_running = false;
    if (s_uac_writer_task) {
        TaskHandle_t t = s_uac_writer_task;
        s_uac_writer_task = NULL;
        // Give the task a moment to exit
        vTaskDelay(pdMS_TO_TICKS(20));
        // (It self-deletes; no vTaskDelete here unless you prefer hard kill)
    }
    if (s_pcm_rb) {
        vRingbufferDelete(s_pcm_rb);
        s_pcm_rb = NULL;
    }
}
