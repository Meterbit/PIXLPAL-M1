#include "stdlib.h"
#include "stdio.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include <cstdint>
#include "esp_system.h"
#include "mtb_audio.h"
#include "mtb_engine.h"
#include "microphone.h"

EXT_RAM_BSS_ATTR TaskHandle_t microphoneProcessing_Task_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t mic_Start_Sem_H = NULL;

//Services *micInProcessing_Sv = new Services(microphone_Task, &microphone_Task_H, "Mic Proc Serv.", 6144, 1, pdTRUE); USE PSRAME AND SEE.
EXT_RAM_BSS_ATTR Services *micInProcessing_Sv = new Services(microphoneProcessing_Task, &microphoneProcessing_Task_H, "Mic Proc Serv.", 6144, 2, pdTRUE, 1);  // USE INTERNAL RAM AND SEE.

void microphoneProcessing_Task(void* d_Service){
  Services *thisServ = (Services *)d_Service;
  init_Mic_DAC_Audio_Processing_Peripherals();
    // Array to store Original audio I2S input stream (reading in chunks, e.g. 1024 values) 
    int16_t audio_buffer[1024];         // 1024 values [2048 bytes] <- for the original I2S signed 16bit stream 
    // now reading the I2S input stream (with NEW <I2S_std.h>)
    size_t bytes_read = 0;

  while (THIS_SERV_IS_ACTIVE == pdTRUE){
  //printf("Microphone service is launched and waiting for activation.\n");
  if(xSemaphoreTake(mic_Start_Sem_H, pdMS_TO_TICKS(50)) != pdTRUE) continue;
  I2S_Record_Init();
    while (mic_OR_dac == I2S_MIC && THIS_SERV_IS_ACTIVE == pdTRUE){
      if(i2s_channel_read(rx_handle, audio_buffer, sizeof(audio_buffer), &bytes_read, pdMS_TO_TICKS(1000)) != ESP_OK) continue;
      // Optionally: Boostering the very low I2S Microphone INMP44 amplitude (multiplying values with factor GAIN_BOOSTER_I2S)
      for (int16_t i = 0; i < (bytes_read / 2); ++i) AudioSamplesTransport.audioBuffer[i] = audio_buffer[i] * GAIN_BOOSTER_I2S;
      AudioSamplesTransport.audioSampleLength_bytes = bytes_read;
      xSemaphoreGive(audio_Data_Collected_Sem_H);
  }
  I2S_Record_De_Init();
  }

  //printf("Microphone Processing Task has exited.\n");
  de_init_Mic_DAC_Audio_Processing_Peripherals();
  kill_This_Service(thisServ);
}