#include "stdlib.h"
#include "stdio.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include <cstdint>
#include "esp_system.h"
#include "mtb_audio.h"
#include "mtb_engine.h"
#include "microphone.h"

static const char TAG[] = "METERBIT_MICROPHONE";

EXT_RAM_BSS_ATTR TaskHandle_t microphoneProcessing_Task_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t mic_Start_Sem_H = NULL;

//Mtb_Services *mtb_Mic_Sv = new Mtb_Services(microphone_Task, &microphone_Task_H, "Mic Proc Serv.", 6144, 1, pdTRUE); USE PSRAME AND SEE.
EXT_RAM_BSS_ATTR Mtb_Services *mtb_Mic_Sv = new Mtb_Services(microphoneProcessing_Task, &microphoneProcessing_Task_H, "Mic Proc Serv.", 6144, 2, pdTRUE, 1);  // USE INTERNAL RAM AND SEE.

void microphoneProcessing_Task(void* d_Service){
  Mtb_Services *thisServ = (Mtb_Services *)d_Service;
  init_Mic_DAC_Audio_Processing_Peripherals();
    // Array to store Original audio I2S input stream (reading in chunks, e.g. 1024 values) 
    int16_t audio_buffer[1024];         // 1024 values [2048 bytes] <- for the original I2S signed 16bit stream 
    // now reading the I2S input stream (with NEW <I2S_std.h>)
    size_t bytes_read = 0;

  while (MTB_SERV_IS_ACTIVE == pdTRUE){
  //ESP_LOGI(TAG, "Microphone service is launched and waiting for activation.\n");
  if(xSemaphoreTake(mic_Start_Sem_H, pdMS_TO_TICKS(50)) != pdTRUE) continue;
  I2S_Record_Init();
    while (mic_OR_dac == I2S_MIC && MTB_SERV_IS_ACTIVE == pdTRUE){
      if(i2s_channel_read(rx_handle, audio_buffer, sizeof(audio_buffer), &bytes_read, pdMS_TO_TICKS(1000)) != ESP_OK) continue;
      // Optionally: Boostering the very low I2S Microphone INMP44 amplitude (multiplying values with factor GAIN_BOOSTER_I2S)
      for (int16_t i = 0; i < (bytes_read / 2); ++i) AudioSamplesTransport.audioBuffer[i] = audio_buffer[i] * GAIN_BOOSTER_I2S;
      AudioSamplesTransport.audioSampleLength_bytes = bytes_read;
      xSemaphoreGive(audio_Data_Collected_Sem_H);
  }
  I2S_Record_De_Init();
  }

  //ESP_LOGI(TAG, "Microphone Processing Task has exited.\n");
  de_init_Mic_DAC_Audio_Processing_Peripherals();
  mtb_End_This_Service(thisServ);
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