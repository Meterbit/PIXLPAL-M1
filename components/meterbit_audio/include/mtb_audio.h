#ifndef MTB_AUDIO_VIS_H
#define MTB_AUDIO_VIS_H

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Audio.h"

//**************************************************************************
#define I2S_MIC                 1
#define I2S_DAC                 0
#define DISABLE_I2S_MIC_DAC     0xFF
#define MIC_DAC_MODULE_PORT     1

// // Digital I/O used
#define I2S_DOUT      40
#define I2S_BCLK      39
#define I2S_LRC       41

#define NO_OF_AUDSPEC_PATTERNS 23

#define SAMPLE_SIZE 1024  // Buffer size of read samples
#define SAMPLE_RATE 16000 // Audio Sample Rate

extern bool completedAudioMicConfig;
extern Audio* audio;

struct AudioSpectVisual_Set_t {
    uint8_t selectedPattern;
    int noOfBands;
    uint8_t showRandom;
    uint8_t randomInterval;
};

extern AudioSpectVisual_Set_t audioSpecVisual_Set;

enum AUDIO_TEXT_DATA_T{
    AUDIO_ID3DATA = 0,
    AUDIO_EOF_MP3,
    AUDIO_SHOW_STATION,
    AUDIO_SHOWS_STREAM_TITLE,
    AUDIO_BITRATE,
    AUDIO_COMMERCIAL,
    AUDIO_ICYURL,
    AUDIO_ICY_DESCRIPTION,
    AUDIO_LASTHOST,
    AUDIO_EOF_SPEECH,
    AUDIO_EOF_STREAM,
    AUDIO_INFO
};

enum AUDIO_I2S_MODE_T{
    CONNECT_HOST = 0,
    OPENAI_SPEECH,
    CONNECT_SPEECH,
    CONNECT_USB_FS,
};

struct AudioTextTransfer_T{
    AUDIO_TEXT_DATA_T Audio_Text_type;
    char Audio_Text_Data[500];
};

extern SemaphoreHandle_t audio_Data_Collected_Sem_H;
extern AUDIO_TEXT_DATA_T selectAudioText;

struct RawAudioData {
    int16_t* audioBuffer = nullptr;
    uint8_t bitsPerSample = 16;
    uint8_t channels = 2;
    size_t audioSampleLength_bytes = 500;
};

extern RawAudioData AudioSamplesTransport;
//extern uint8_t VisualizeAudio;

extern TimerHandle_t showRandomPatternTimer_H;
extern TaskHandle_t audioProcessing_Task_H;
extern QueueHandle_t audioProcessing_Q_H;
extern QueueHandle_t audioTextInfo_Q_H;
extern SemaphoreHandle_t dac_Start_Sem_H;

extern void audioProcessing_Task(void *params);
extern void audioVisualizer();
//extern esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

extern TaskHandle_t microphone_Task_H;
extern SemaphoreHandle_t mic_Start_Sem_H;
//extern SemaphoreHandle_t usbSpk_Ready_Sem_H;
extern void microphoneProcessing_Task(void *);
//extern void usb_Speaker_Task(void *);
extern uint8_t mic_OR_dac;
extern void mtb_Use_Mic_Or_Dac(uint8_t);
extern void randomPattern_TimerCallback(TimerHandle_t dHandle);
extern void init_Mic_DAC_Audio_Processing_Peripherals(void);
extern void de_init_Mic_DAC_Audio_Processing_Peripherals(void);


class MTB_Audio {
    public:
        MTB_Audio(){};

        int8_t contdSucceed = -1; // Two (-1) here stands for a number that is neither 0 or 1.

        String speech_Message;
        String openAI_Model;
        String openAI_Voice;
        String openAI_ResponseFormat;
        String openAI_Speed;

        String host_Url;
        String host_Username;
        String host_Password;

        String ggle_Lang;

        String filePath;
        int32_t fileStartPos;

        bool mtb_Openai_Speech(const String &model, const String &input, const String &voice, const String &response_format, const String &speed);
        bool mtb_ConnectToHost(const char *host, const char *user = "", const char *pwd = "");
        bool mtb_ConnectToSpeech(const char *speech, const char *lang);
        bool mtb_ConnectToUSB_FS(const char *path, int32_t m_fileStartPos = -1);
        //bool mtb_ConnectToUSBFS(const char *path, int32_t m_fileStartPos = -1);
};

extern MTB_Audio *mtb_audioPlayer;

#endif