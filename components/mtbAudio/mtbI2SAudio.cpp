
#include "stdlib.h"
#include "stdio.h"
#include "arduinoFFT.h"
#include <math.h>
#include <time.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "FFT.h"
#include "nvsMem.h"
#include "ledDriver.h"
#include "patternsHUB75.h"
#include "mtbAudio.h"
#include "lib8tion.h"
#include "mtbApps.h"
#include <LittleFS.h>
#include "USBFS.h"   // make sure this is in your include path

EXT_RAM_BSS_ATTR TimerHandle_t showRandomPatternTimer_H = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t audioProcessing_Task_H = NULL;
EXT_RAM_BSS_ATTR QueueHandle_t audioTextInfo_Q_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t dac_Start_Sem_H = NULL;
EXT_RAM_BSS_ATTR SemaphoreHandle_t audio_Data_Collected_Sem_H = NULL;

AudioSpectVisual_Set_t audioSpecVisual_Set{
    .selectedPattern = 0,
    .noOfBands = 32,
    .showRandom = 0,
    .randomInterval = 4
};

bool completedAudioMicConfig = false;
AUDIO_I2S_MODE_T audioOutMode = CONNECT_HOST;
Audio *audio = nullptr;
MTB_Audio *mtbAudioPlayer = nullptr;
uint8_t mic_OR_dac = DISABLE_I2S_MIC_DAC;
RawAudioData AudioSamplesTransport;
//AUDIO_TEXT_DATA_T selectAudioText = AUDIO_INFO;
AudioTextTransfer_T audioTextTransferBuffer;

//uint8_t VisualizeAudio = false;
uint8_t kMatrixWidth = 128; //   PANEL_WIDTH
uint8_t kMatrixHeight = 64; // PANEL_HEIGHT
int loopcounter = 0;

EXT_RAM_BSS_ATTR Services *audioOutProcessing_Sv = new Services(audioProcessing_Task, &audioProcessing_Task_H, "Aud Pro Serv.", 6144, 2, pdFALSE, 1);

void audioProcessing_Task(void *d_Service){
  Services *thisServ = (Services *)d_Service;
  String openai_key = "sk-svcacct-7TDOdEhvwntW7SrC1brgBH2VNmqXXhxKh2bLjobDdiecWv82WIvnNe_yQaFugvaoJFsIT3BlbkFJIa5_gMWvZw31-rfvKtWTAwNAnAK6uMRX1_FV2O-tmixbxpZ5Nfx40dKnHJSMSsaR6VQA";
  mtbAudioPlayer = new MTB_Audio();
  init_Mic_DAC_Audio_Processing_Peripherals();
  if(audioTextInfo_Q_H == NULL) audioTextInfo_Q_H = xQueueCreate(5, sizeof(AudioTextTransfer_T));
  read_struct_from_nvs("dev_Volume", &deviceVolume, sizeof(uint8_t));
  while (THIS_SERV_IS_ACTIVE == pdTRUE){
    if(xSemaphoreTake(dac_Start_Sem_H, pdMS_TO_TICKS(50)) != pdTRUE) continue;
    audio = new Audio();
    audio->setConnectionTimeout(65000, 65000);

    switch(audioOutMode){
      case CONNECT_HOST: mtbAudioPlayer->contdSucceed = (int8_t) audio->connecttohost(mtbAudioPlayer->host_Url.c_str(), mtbAudioPlayer->host_Username.c_str(), mtbAudioPlayer->host_Password.c_str());
        break;
      case OPENAI_SPEECH: mtbAudioPlayer->contdSucceed = (int8_t) audio->openai_speech(openai_key , mtbAudioPlayer->openAI_Model, mtbAudioPlayer->speech_Message, mtbAudioPlayer->openAI_Voice,mtbAudioPlayer->openAI_ResponseFormat, mtbAudioPlayer->openAI_Speed);
        break;
      case CONNECT_SPEECH: mtbAudioPlayer->contdSucceed = (int8_t) audio->connecttospeech(mtbAudioPlayer->speech_Message.c_str(), mtbAudioPlayer->ggle_Lang.c_str());
        break;
      case CONNECT_LittleFS: mtbAudioPlayer->contdSucceed = (int8_t) audio->connecttoFS(USBFS, mtbAudioPlayer->filePath.c_str(), mtbAudioPlayer->fileStartPos);
        break;
      default: ESP_LOGE("MTB_AUDIO", "Audio Output Mode Not Specified.");
      goto KillAudio;
      break;
    }

      audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
      audio->setVolume(deviceVolume);
      
    while(mic_OR_dac == I2S_DAC && THIS_SERV_IS_ACTIVE == pdTRUE){
      audio->loop();
      vTaskDelay(1);
    }// loop end
    KillAudio:
      delay(100);
      delete audio;
      audio = nullptr;
  }
  delete mtbAudioPlayer;
  //vQueueDelete(audioTextInfo_Q_H); audioTextInfo_Q_H = NULL; //The Queue remains active for the next audio service. This prevents apps such as the Internet Radio from crashing.
  de_init_Mic_DAC_Audio_Processing_Peripherals();
  kill_This_Service(thisServ);
} 

void init_Mic_DAC_Audio_Processing_Peripherals(void){
  if(mic_Start_Sem_H == NULL) mic_Start_Sem_H = xSemaphoreCreateBinary();
  if(dac_Start_Sem_H == NULL) dac_Start_Sem_H = xSemaphoreCreateBinary();
  //if(usbSpk_Ready_Sem_H == NULL) usbSpk_Ready_Sem_H = xSemaphoreCreateBinary(); // To reuse USB Speaker, uncomment this line and get library from Archive @ 23 Jan 2025
  if(audio_Data_Collected_Sem_H == NULL) audio_Data_Collected_Sem_H = xSemaphoreCreateBinary();  
//************************************************************************************************************************************ */
  if(AudioSamplesTransport.audioBuffer == nullptr) AudioSamplesTransport.audioBuffer = (int16_t*) calloc(SAMPLEBLOCK, sizeof(int16_t));
  if(vReal == nullptr) vReal = (double*)calloc(SAMPLEBLOCK, sizeof(double));
  if(vImag == nullptr) vImag = (double*)calloc(SAMPLEBLOCK, sizeof(double));
  if(FreqBins == nullptr) FreqBins = (float*)calloc(65, sizeof(float));
  if(oldBarHeights == nullptr) oldBarHeights = (int*)calloc(65, sizeof(int));
  if(peak == nullptr) peak = (uint8_t*)calloc(65, sizeof(uint8_t));
  if(PeakFlag == nullptr) PeakFlag = (char*)calloc(64, sizeof(char));
  if(PeakTimer == nullptr) PeakTimer = (int*)calloc(64, sizeof(int));

  srand(time(NULL));
  read_struct_from_nvs("audioSpecSet", &audioSpecVisual_Set, sizeof(AudioSpectVisual_Set_t));
  if(showRandomPatternTimer_H == NULL) showRandomPatternTimer_H = xTimerCreate("rand pat tim", pdMS_TO_TICKS(audioSpecVisual_Set.randomInterval * 1000), pdTRUE, NULL, randomPattern_TimerCallback);
}

void de_init_Mic_DAC_Audio_Processing_Peripherals(void){
  if(AudioSamplesTransport.audioBuffer != nullptr) free(AudioSamplesTransport.audioBuffer); 
  AudioSamplesTransport.audioBuffer = nullptr;
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

void use_Mic_OR_Dac(uint8_t mtb_i2s_Module){
  mic_OR_dac = mtb_i2s_Module;
  delay(100);
  switch(mic_OR_dac){
    case I2S_DAC:xSemaphoreGive(dac_Start_Sem_H);
      break;
    case I2S_MIC: xSemaphoreGive(mic_Start_Sem_H);
      break;
    default:  printf("Mic and/or Speakers Deactivated. \n");
      break;
    }
}

// void audio_info(const char *info){
//     printf("AudioI2S Info: %s \n",  info);
//     // audioTextTransferBuffer.Audio_Text_type = AUDIO_INFO;
//     // strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     // xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_id3data(const char *info){  //id3 metadata
//     //printf("id3Data: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_ID3DATA;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_eof_mp3(const char *info){  //end of mp3 file
//     //printf("eof_mp3: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_EOF_MP3;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
void audio_showstation(const char *info){
    //printf("Station: %s \n",  info);
    audioTextTransferBuffer.Audio_Text_type = AUDIO_SHOW_STATION;
    strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
    xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
}
void audio_showstreamtitle(const char *info){
    //printf("Stream Title: %s \n",  info);
    audioTextTransferBuffer.Audio_Text_type = AUDIO_SHOWS_STREAM_TITLE;
    strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
    xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
}
// void audio_bitrate(const char *info){
//     //printf("Bitrate: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_BITRATE;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_commercial(const char *info){  //duration in sec
//     //printf("Commercial: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_COMMERCIAL;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_icyurl(const char *info){  //homepage
//     //printf("ICYURL: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_ICYURL;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_icydescription(const char* info){
//     //printf("ICY DESCRIPTION: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_ICY_DESCRIPTION;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
// void audio_lasthost(const char *info){  //stream URL played
//     //printf("LastHost: %s \n",  info);
//     audioTextTransferBuffer.Audio_Text_type = AUDIO_LASTHOST;
//     strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
//     xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
// }
void audio_eof_speech(const char *info){
    printf("End Of Speeach: %s \n",  info);
    audioTextTransferBuffer.Audio_Text_type = AUDIO_EOF_SPEECH;
    strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
    xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
}

void audio_eof_stream(const char* info){     // The webstream comes to an end
    printf("End Of Stream: %s \n",  info);
    audioTextTransferBuffer.Audio_Text_type = AUDIO_EOF_STREAM;
    strcpy(audioTextTransferBuffer.Audio_Text_Data, info);
    xQueueSend(audioTextInfo_Q_H, &audioTextTransferBuffer, 0);
} 

// void audio_process_i2s(int16_t* outBuff, uint16_t validSamples, uint8_t bitsPerSample, uint8_t channels, bool *continueI2S){ // record audiodata or send via BT
//   memcpy(AudioSamplesTransport.audioBuffer, outBuff, (size_t)validSamples);
//   AudioSamplesTransport.audioSampleLength_bytes = validSamples;
//   xSemaphoreGive(audio_Data_Collected_Sem_H);
//   printf("Bits PerSample: %d,  No of channels: %d, no of valid samples %d\n", bitsPerSample, channels, validSamples);
//    *continueI2S = true;
// }

// void audio_id3image(File& file, const size_t pos, const size_t size){ //ID3 metadata image

// }

// void audio_id3lyrics(File& file, const size_t pos, const size_t size){ //ID3 metadata lyrics

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
    //  Serial.printf( " - log: %d \n",(int)FreqBins[i]);
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
        // case PATTERN_23 :
        //  PeakDirection = AUD_VIS_UP;
        //  TriBars(band, barHeight);
        //  TriPeak(band);
        //    break;
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
 EVERY_N_MILLISECONDS(Fallingspeed){
   for (uint8_t band = 0; band < audioSpecVisual_Set.noOfBands; band++){
     if(PeakFlag[band]==1){
       PeakTimer[band]++;
       if (PeakTimer[band]> Peakdelay){PeakTimer[band]=0;PeakFlag[band]=0;}
     }
     else if ((peak[band] > 0) &&(PeakDirection==AUD_VIS_UP)){ 
       peak[band] += 1;
       if (peak[band]>(kMatrixHeight+10))peak[band]=0;
       } // when to far off screen then reset peak height
     else if ((peak[band] > 0)&&(PeakDirection==AUD_VIS_DOWN)){ peak[band] -= 1;}
   }   
 }
}

 void randomPattern_TimerCallback(TimerHandle_t dHandle){
  audioSpecVisual_Set.selectedPattern = rand() % NO_OF_AUDSPEC_PATTERNS;
 }

bool MTB_Audio::mtb_Openai_Speech(const String& model, const String& input, const String& voice, const String& response_format, const String& speed){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;
  
  openAI_Model = model;
  speech_Message = input;
  openAI_Voice = voice;
  openAI_ResponseFormat = response_format;
  openAI_Speed = speed;
  audioOutMode = OPENAI_SPEECH;

  use_Mic_OR_Dac(I2S_DAC);

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
  use_Mic_OR_Dac(I2S_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}

bool MTB_Audio::mtb_ConnectToSpeech(const char* speech, const char* lang){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;

  speech_Message = String(speech);
  ggle_Lang = String(lang);

  audioOutMode = CONNECT_SPEECH;
  use_Mic_OR_Dac(I2S_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}

bool MTB_Audio::mtb_ConnectToLittleFS( const char *path, int32_t m_fileStartPos){
  int16_t countdown = 2000;   // 2000 here represents the number of 5ms in 10s
  contdSucceed = -1;
  
  filePath = String(path);
  fileStartPos = m_fileStartPos;

  audioOutMode = CONNECT_LittleFS;
  use_Mic_OR_Dac(I2S_DAC);

  while (contdSucceed == -1 && countdown-->0) delay(5);
  return (bool) contdSucceed;
}