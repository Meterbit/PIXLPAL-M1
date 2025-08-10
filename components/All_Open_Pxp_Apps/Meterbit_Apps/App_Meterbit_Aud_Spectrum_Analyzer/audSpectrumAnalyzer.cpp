
#include "stdlib.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "arduinoFFT.h"
#include <math.h>
#include "mtb_nvs.h"
#include "audSpectrumAnalyzer.h"
#include "mtb_audio.h"
#include "mtb_engine.h"

#define AUD_SPEC_UP 1
#define AUD_SPEC_DOWN 0

static const char TAG[] = "AUD_SPEC_ANALYZER";

void changePattern_Button(button_event_t);

void selectNumOfBands(JsonDocument&);
void selectPattern(JsonDocument&);
void setRandomPatterns(JsonDocument&);
void setRandomInterval(JsonDocument&);

EXT_RAM_BSS_ATTR TaskHandle_t audSpecAnalyzer_MQTT_Parser_Task_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t audSpecAnalyzer_Task_H = NULL;

EXT_RAM_BSS_ATTR Applications_FullScreen *audSpecAnalyzer_App = new Applications_FullScreen(audSpecAnalyzer_App_Task, &audSpecAnalyzer_Task_H, "Audio Spectr", 10240);
uint32_t counterValue1 = 0;
//***************************************************************************************************
void  audSpecAnalyzer_App_Task(void* dApplication){
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = mtb_Brightness_Control;
  thisApp->mtb_App_ButtonFn_ptr = changePattern_Button;
  mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(selectPattern, selectNumOfBands, setRandomPatterns, setRandomInterval);
  mtb_App_Init(thisApp, mtb_Mic_Sv);
  //**************************************************************************************************************************
  if(audioSpecVisual_Set.showRandom) xTimerStart(showRandomPatternTimer_H, 0);
  delay(200);   // This delay is played here to allow for the creation of of mic processing variables and initialization.
  mtb_Use_Mic_Or_Dac(I2S_MIC);
  while (MTB_APP_IS_ACTIVE == pdTRUE){
    if(xSemaphoreTake(audio_Data_Collected_Sem_H, pdMS_TO_TICKS(50)) == pdTRUE) audioVisualizer();
  }
  mtb_Use_Mic_Or_Dac(DISABLE_I2S_MIC_DAC);
  mtb_End_This_App(thisApp);
}

void changePattern_Button(button_event_t button_Data){
        switch (button_Data.type){
        case BUTTON_RELEASED:
        break;

        case BUTTON_PRESSED:
          ++audioSpecVisual_Set.selectedPattern;
          audioSpecVisual_Set.selectedPattern %= NO_OF_AUDSPEC_PATTERNS;  // 23 HERE REPS THE NUMBER OF PATTERNS THAT HAVE BEEN CREATED AND READY FOR DISPLAY
          mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
          break;

        case BUTTON_PRESSED_LONG:
          break;

        case BUTTON_CLICKED:
        //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
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

void selectPattern(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  audioSpecVisual_Set.selectedPattern = dCommand["selectedPattern"];
  mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));       
  mtb_Ble_App_Cmd_Respond_Success(audSpecAnalyzerAppRoute, cmd, pdPASS);
}

void selectNumOfBands(JsonDocument& dCommand){
  uint8_t bandChoice[] = {1, 2, 3, 4, 8};
  uint8_t cmd = dCommand["app_command"];
  uint8_t selectBand = dCommand["numOfBands"];
  audioSpecVisual_Set.noOfBands = bandChoice[selectBand] * 8;
  mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
  ESP_LOGI(TAG, "No of bands selected: %d \n", audioSpecVisual_Set.noOfBands);
  mtb_Ble_App_Cmd_Respond_Success(audSpecAnalyzerAppRoute, cmd, pdPASS);
}

void setRandomPatterns(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  audioSpecVisual_Set.showRandom = dCommand["showRandom"];
  if(audioSpecVisual_Set.showRandom) xTimerStart(showRandomPatternTimer_H, 0);
  else xTimerStop(showRandomPatternTimer_H, 0);
  mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));        
  mtb_Ble_App_Cmd_Respond_Success(audSpecAnalyzerAppRoute, cmd, pdPASS);
}

void setRandomInterval(JsonDocument& dCommand){
  uint8_t cmd = dCommand["app_command"];
  audioSpecVisual_Set.randomInterval = dCommand["randomInterval"];
  xTimerDelete(showRandomPatternTimer_H, pdMS_TO_TICKS(10));
  showRandomPatternTimer_H = xTimerCreate("rand pat tim", pdMS_TO_TICKS(audioSpecVisual_Set.randomInterval * 1000), pdTRUE, NULL, randomPattern_TimerCallback);
  xTimerStart(showRandomPatternTimer_H, 0);
  mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
  mtb_Ble_App_Cmd_Respond_Success(audSpecAnalyzerAppRoute, cmd, pdPASS);
}


// void setSensitivity(JsonDocument& dCommand){
//   uint8_t cmd = dCommand["app_command"];
//   audioSpecVisual_Set.sensitivity = dCommand["sensitivity"];
//   //Compute audioSpecVisual_Set.sensitivity
//   mtb_Write_Nvs_Struct("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
//   mtb_Ble_App_Cmd_Respond_Success(audSpecAnalyzerAppRoute, cmd, pdPASS);
// }