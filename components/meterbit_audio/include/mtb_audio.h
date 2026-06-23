/**
 * @file mtb_audio.h
 * @brief Audio subsystem API for the PIXLPAL-M1, covering I2S playback, microphone
 *        capture, spectrum visualisation, and OpenAI / Google TTS integration.
 *
 * The audio output pipeline is started with init_Audio_Out_Processing() and torn down
 * with de_init_Audio_Out_Processing(). The microphone input pipeline is managed
 * symmetrically with init_Audio_In_Processing() / de_init_Audio_In_Processing().
 * The global instance @c mtb_audioPlayer is used for all playback operations.
 */
#ifndef MTB_AUDIO_VIS_H
#define MTB_AUDIO_VIS_H

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Audio.h"

/** @defgroup audio_io_mode Audio I/O Mode Flags
 *  @brief Flags selecting between microphone capture and DAC/speaker output.
 *  @{
 */
#define ON_MIC              1    /**< Route audio pipeline to the I2S microphone. */
#define ON_DAC              0    /**< Route audio pipeline to the I2S DAC (speaker). */
#define OFF_DAC_N_MIC    0xFF   /**< Disable both microphone and DAC. */
#define MIC_DAC_MODULE_PORT 1   /**< I2S port number shared by the microphone and DAC modules. */
/** @} */

/** @defgroup audio_i2s_pins I2S Output Pin Assignments
 *  @{
 */
#define I2S_DOUT  40  /**< GPIO pin for I2S serial data output to the DAC. */
#define I2S_BCLK  39  /**< GPIO pin for I2S bit clock. */
#define I2S_LRC   41  /**< GPIO pin for I2S left/right (word-select) clock. */
/** @} */

#define NO_OF_AUDSPEC_PATTERNS 23  /**< Total number of audio spectrum visualiser patterns. */

#define SAMPLE_SIZE  1024   /**< Number of PCM samples per microphone capture buffer. */
#define SAMPLE_RATE  16000  /**< I2S microphone sample rate in Hz. */

extern bool   completedAudioMicConfig;  /**< True once the I2S microphone channel has been configured. */
extern Audio* audio;                    /**< Pointer to the global Arduino Audio library instance. */

//static const String openai_key = "INSERT YOUR OPENAI KEY HERE";

/**
 * @brief Runtime settings for the audio spectrum visualiser.
 */
struct AudioSpectVisual_Set_t {
    uint8_t selectedPattern;  /**< Index of the currently active pattern (0 – NO_OF_AUDSPEC_PATTERNS-1). */
    int     noOfBands;        /**< Number of frequency bands rendered on the panel. */
    uint8_t showRandom;       /**< Non-zero to cycle through patterns automatically. */
    uint8_t randomInterval;   /**< Interval in seconds between automatic pattern changes. */
};

extern AudioSpectVisual_Set_t audioSpecVisual_Set;  /**< Global spectrum visualiser settings instance. */

/**
 * @brief Audio metadata text event types posted to audioTextInfo_Q.
 */
enum AUDIO_TEXT_DATA_T {
    AUDIO_ID3DATA = 0,        /**< ID3 tag metadata from an MP3 stream. */
    AUDIO_EOF_MP3,            /**< End-of-file reached for a local MP3 file. */
    AUDIO_SHOW_STATION,       /**< Internet radio station name string. */
    AUDIO_SHOWS_STREAM_TITLE, /**< Currently playing stream title. */
    AUDIO_BITRATE,            /**< Stream bitrate information string. */
    AUDIO_COMMERCIAL,         /**< Commercial / advertisement boundary marker. */
    AUDIO_ICYURL,             /**< ICY stream URL. */
    AUDIO_ICY_DESCRIPTION,    /**< ICY stream description text. */
    AUDIO_ICYLOGO,            /**< ICY stream logo URL. */
    AUDIO_LASTHOST,           /**< URL of the last successfully connected host. */
    AUDIO_EOF_SPEECH,         /**< End of TTS speech synthesis audio. */
    AUDIO_EOF_STREAM,         /**< End of streaming audio reached. */
    AUDIO_INFO,               /**< General informational message from the audio library. */
    AUDIO_SYNC_LYRICS,        /**< Synchronised lyrics payload. */
    AUDIO_LOG                 /**< Internal audio library log message. */
};

/**
 * @brief I2S audio output modes supported by MTB_Audio.
 */
enum AUDIO_I2S_MODE_T {
    CONNECT_HOST = 0, /**< Stream audio from an HTTP/HTTPS URL. */
    OPENAI_SPEECH,    /**< Synthesise speech via the OpenAI TTS API. */
    CONNECT_SPEECH,   /**< Use Google or on-device TTS for speech output. */
    CONNECT_USB_FS,   /**< Play an audio file from the USB / LittleFS filesystem. */
};

/**
 * @brief Packet posted to audioTextInfo_Q carrying a text payload from the audio library.
 */
struct AudioTextTransfer_T {
    AUDIO_TEXT_DATA_T Audio_Text_type;   /**< Category of the enclosed text data. */
    char              Audio_Text_Data[500]; /**< Null-terminated text payload (max 499 chars). */
};

extern SemaphoreHandle_t audio_In_Data_Collected_Sem_H; /**< Given when a microphone buffer is ready for processing. */
extern AUDIO_TEXT_DATA_T selectAudioText;               /**< Selects which metadata type to forward to the display. */

/**
 * @brief Container for a raw PCM audio buffer captured from the I2S microphone.
 */
struct RawAudioData {
    int16_t* audioBuffer = nullptr;         /**< Pointer to heap-allocated PCM sample data. */
    uint8_t  bitsPerSample = 16;            /**< Bit depth per sample (8 or 16). */
    uint8_t  channels = 2;                  /**< Channel count (1 = mono, 2 = stereo). */
    size_t   audioSampleLength_bytes = 500; /**< Size of @p audioBuffer in bytes. */
};

extern RawAudioData AudioSamplesTransport; /**< Shared buffer transferring captured PCM data to the visualiser task. */

//extern uint8_t VisualizeAudio;

extern TimerHandle_t     showRandomPatternTimer_H;   /**< Software timer that fires to advance the visualiser pattern. */
extern TaskHandle_t      audioProcessing_Task_H;     /**< Handle for the I2S audio output processing task. */
extern TaskHandle_t      usb_Speaker_Process_Task_H; /**< Handle for the USB speaker processing task. */
extern QueueHandle_t     audioProcessing_Q_H;        /**< Queue feeding PCM data to the audio output task. */
extern QueueHandle_t     audioTextInfo_Q;            /**< Queue delivering AudioTextTransfer_T metadata packets to the app. */
extern SemaphoreHandle_t dac_Start_Sem_H;            /**< Binary semaphore released to start the DAC/I2S output task. */

/**
 * @brief Callback invoked by the Audio library when metadata or status text is available.
 * @param m  Audio library message struct containing event type and text payload.
 */
extern void mtb_Audio_Info(Audio::msg_t m);

static void audio_Out_Processing_Task(void *d_Service);
static void audio_In_Processing_Task(void *d_Service);
//extern void usb_Speaker_Process_Task(void *params);

/**
 * @brief Runs one frame of the audio spectrum visualiser, reading FFT results and drawing the active pattern.
 * @note  Call this from within an app's update loop while the visualiser is active.
 */
extern void audioVisualizer();
//extern esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);

extern TaskHandle_t      microphone_Task_H; /**< Handle for the I2S microphone capture task. */
extern SemaphoreHandle_t mic_Start_Sem_H;   /**< Binary semaphore released to start microphone capture. */
//extern SemaphoreHandle_t usbSpk_Ready_Sem_H;

/**
 * @brief FreeRTOS task that reads PCM samples from the I2S microphone and posts them for processing.
 * @param param  Unused task parameter.
 */
extern void microphoneProcessing_Task(void*);
//extern void usb_Speaker_Task(void *);

extern uint8_t mic_OR_dac; /**< Current audio routing state: ON_MIC, ON_DAC, or OFF_DAC_N_MIC. */

/**
 * @brief Switch the active audio hardware between the microphone, DAC speaker, or neither.
 * @param mode  Desired routing: ON_MIC, ON_DAC, or OFF_DAC_N_MIC.
 */
extern void mtb_Dac_Or_Mic_Status(uint8_t mode);

/**
 * @brief FreeRTOS timer callback that advances the visualiser to the next random pattern.
 * @param dHandle  FreeRTOS timer handle (unused inside the callback).
 */
extern void randomPattern_TimerCallback(TimerHandle_t dHandle);

/**
 * @brief Initialise the I2S audio output pipeline and start the playback task.
 */
extern void init_Audio_Out_Processing(void);

/**
 * @brief Tear down the I2S audio output pipeline and stop the playback task.
 */
extern void de_init_Audio_Out_Processing(void);

/**
 * @brief Initialise the I2S microphone capture pipeline and start the recording task.
 */
extern void init_Audio_In_Processing(void);

/**
 * @brief Tear down the I2S microphone capture pipeline and stop the recording task.
 */
extern void de_init_Audio_In_Processing(void);

/**
 * @brief Initialise the spectrum visualiser, allocating FFT buffers and starting the pattern timer.
 */
extern void initAudioVisualPattern(void);

/**
 * @brief Tear down the spectrum visualiser and free its FFT buffers.
 */
extern void deInitAudioVisualPattern(void);

/**
 * @brief Write a block of USB audio samples directly to the I2S DAC output.
 * @param buffer  Pointer to the PCM sample buffer.
 * @param length  Number of bytes in @p buffer.
 */
extern void mtb_Play_USB_Audio_Samples(void* buffer, size_t length);

/**
 * @brief High-level audio player wrapping the Arduino ESP32 Audio library.
 *
 * Supports streaming from HTTP hosts, OpenAI TTS, Google TTS, and local USB/LittleFS
 * audio files. Use the global instance @c mtb_audioPlayer rather than creating
 * additional instances. Initialise the output pipeline with init_Audio_Out_Processing()
 * before calling any connection method.
 *
 * @note Only one playback mode is active at a time. Call de_init_Audio_Out_Processing()
 *       before switching modes to avoid I2S state conflicts.
 */
class MTB_Audio {
    public:
        MTB_Audio(){};

        int8_t contdSucceed = -1; /**< Last connection result: 1 = success, 0 = fail, -1 = not yet attempted. */

        String speech_Message; /**< Text to synthesise in the next TTS request. */

        /** @name OpenAI TTS Settings */
        /**@{*/
        String openAI_Instructions;   /**< Optional system instruction for voice characteristics. */
        String openAI_Model;          /**< OpenAI TTS model name (e.g. "tts-1" or "tts-1-hd"). */
        String openAI_Voice;          /**< Voice selection (e.g. "alloy", "echo", "nova", "shimmer"). */
        String openAI_ResponseFormat; /**< Audio format returned by the API (e.g. "mp3", "opus"). */
        String openAI_Speed;          /**< Playback speed factor as a string ("0.25" – "4.0"). */
        /**@}*/

        /** @name HTTP Host Streaming Settings */
        /**@{*/
        String host_Url;      /**< Full HTTP/HTTPS URL to the audio stream. */
        String host_Username; /**< Optional HTTP Basic Auth username. */
        String host_Password; /**< Optional HTTP Basic Auth password. */
        /**@}*/

        /** @name Google TTS Settings */
        /**@{*/
        String ggle_Lang; /**< BCP-47 language code for Google speech (e.g. "en-US"). */
        /**@}*/

        /** @name USB / LittleFS Playback Settings */
        /**@{*/
        String  filePath;     /**< Path to the audio file (e.g. "/audio/track.mp3"). */
        int32_t fileStartPos; /**< Byte offset to begin playback from (-1 = start of file). */
        /**@}*/

        // OPENAI SPEECH PARAMETERS:
        // Text to speech API provides a speech endpoint based on our TTS (text-to-speech) model.
        // More info: https://platform.openai.com/docs/guides/text-to-speech/text-to-speech

        // Request body:
        // model (string) [Required] - One of the available TTS models: tts-1 or tts-1-hd
        // input (string) [Required] - The text to generate audio for. The maximum length is 4096 characters.
        // instructions (string) [Optional] - A description of the desired characteristics of the generated audio.
        // voice (string) [Required] - The voice to use when generating the audio. Supported voices are alloy, echo, fable, onyx, nova, and shimmer.
        // response_format (string) [Optional] - Defaults to mp3. The format to audio in. Supported formats are mp3, opus, aac, and flac.
        // speed (number) [Optional] - Defaults to 1. The speed of the generated audio. Select a value from 0.25 to 4.0. 1.0 is the default.

        /**
         * @brief Request speech synthesis from the OpenAI TTS API.
         * @param model            TTS model ("tts-1" or "tts-1-hd").
         * @param input            Text to synthesise (max 4096 characters).
         * @param instructions     Optional voice characteristics instruction string.
         * @param voice            Voice name ("alloy", "echo", "fable", "onyx", "nova", "shimmer").
         * @param response_format  Output audio format ("mp3", "opus", "aac", "flac").
         * @param speed            Playback speed as a string ("0.25" – "4.0").
         * @return true  if the request was accepted and audio playback started.
         * @return false if the HTTP request failed or the audio library rejected the stream.
         */
        bool mtb_Openai_Speech(const String &model, const String &input, const String& instructions, const String &voice, const String &response_format, const String &speed);

        /**
         * @brief Open an HTTP/HTTPS audio stream for playback.
         * @param host  Full stream URL.
         * @param user  Optional HTTP Basic Auth username (default "").
         * @param pwd   Optional HTTP Basic Auth password (default "").
         * @return true  if the connection was established and streaming began.
         * @return false on connection or audio library failure.
         */
        bool mtb_ConnectToHost(const char *host, const char *user = "", const char *pwd = "");

        /**
         * @brief Synthesise and play speech using the Google TTS service.
         * @param speech  Text string to synthesise.
         * @param lang    BCP-47 language code (e.g. "en-US").
         * @return true  if playback started successfully.
         * @return false on network or audio error.
         */
        bool mtb_ConnectToSpeech(const char *speech, const char *lang);

        /**
         * @brief Play an audio file from the USB / LittleFS filesystem.
         * @param path           File path (e.g. "/audio/track.mp3").
         * @param m_fileStartPos Byte offset to seek before playback (-1 = beginning of file).
         * @return true  if the file was opened and playback started.
         * @return false if the file could not be opened or the audio library failed.
         */
        bool mtb_ConnectToUSB_FS(const char *path, int32_t m_fileStartPos = -1);

};

extern MTB_Audio *mtb_audioPlayer; /**< Global audio player instance; initialised in init_Audio_Out_Processing(). */

#endif
