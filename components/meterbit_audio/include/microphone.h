/**
 * @file microphone.h
 * @brief INMP441 I2S microphone driver configuration and recording API.
 *
 * Configures an I2S standard channel for the INMP441 digital MEMS microphone.
 * Provides I2S_Record_Init() / I2S_Record_De_Init() to start and stop recording.
 * Include this header in exactly ONE translation unit — it instantiates the I2S
 * config struct and global state variables directly.
 *
 * @note The GAIN_BOOSTER_I2S multiplier compensates for the very low output level
 *       of the INMP441; tune it to match your acoustic environment.
 */




#ifndef MICROPHONE_H
#define MICROPHONE_H

#include "driver/i2s_std.h"     // instead of older legacy #include <driver/i2s.h>

// --- defines & macros --------

#ifndef DEBUG                   // user can define favorite behaviour ('true' displays addition info)
#  define DEBUG true            // <- define your preference here
#  define DebugPrint(x);        if(DEBUG){Serial.print(x);}   /* do not touch */
#  define DebugPrintln(x);      if(DEBUG){Serial.println(x);} /* do not touch */
#endif


/** @defgroup mic_pins I2S Microphone GPIO Pin Assignments
 *  @{
 */
#define I2S_WS   47  /**< Word-select (L/R) pin; INMP441 on Vcc = RIGHT channel, on GND = LEFT channel. */
#define I2S_SD   14  /**< Serial data input from the microphone. */
#define I2S_SCK  21  /**< Serial clock (bit clock) output to the microphone. */
/** @} */

/** @defgroup mic_settings I2S Recording Settings
 *  @{
 */
#define SAMPLE_RATE      16000  /**< ADC sampling frequency in Hz (best quality at 16000 or 24000). */
                                /**< Above 24000 Hz random dropouts and distortion may occur. */

#define BITS_PER_SAMPLE  16     /**< Bit depth: 8 or 16 bit supported; 24/32 bit not supported. */
                                /**< 8-bit combined with a low sample rate gives the fastest STT. */

#define GAIN_BOOSTER_I2S 32     /**< PCM amplitude multiplier applied after I2S read (1–64). */
                                /**< 32 is the recommended value for INMP441; 64 adds more noise. */
/** @} */


/**
 * @brief I2S standard-mode channel configuration for the INMP441 microphone.
 *
 * Configured for 16-bit mono LEFT-channel capture at SAMPLE_RATE Hz with
 * Philips/PCM bit-shift framing (bit_shift = true).
 */
i2s_std_config_t  std_cfg =
{ .clk_cfg  =   // instead of macro 'I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),'
  { .sample_rate_hz = SAMPLE_RATE,
    .clk_src = I2S_CLK_SRC_DEFAULT,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
  },
  .slot_cfg =   // instead of macro I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
  { // hint: always using _16BIT because I2S uses 16 bit slots anyhow (even in case I2S_DATA_BIT_WIDTH_8BIT used !)
    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,   // not I2S_DATA_BIT_WIDTH_8BIT or (i2s_data_bit_width_t) BITS_PER_SAMPLE
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
    .slot_mode = I2S_SLOT_MODE_MONO,              // or I2S_SLOT_MODE_STEREO
    .slot_mask = I2S_STD_SLOT_LEFT,              // use 'I2S_STD_SLOT_LEFT' in case L/R pin is connected to GND !
    .ws_width =  I2S_DATA_BIT_WIDTH_16BIT,
    .ws_pol = false,
    .bit_shift = true,   // using [.bit_shift = true] similar PHILIPS or PCM format (NOT 'false' as in MSB macro) ! ..
    //.msb_right = false,  // .. or [.msb_right = true] to avoid overdriven and distorted signals on I2S microphones
  },
  .gpio_cfg =
  { .mclk = I2S_GPIO_UNUSED,
    .bclk = (gpio_num_t) I2S_SCK,
    .ws   = (gpio_num_t) I2S_WS,
    .dout = I2S_GPIO_UNUSED,
    .din  = (gpio_num_t) I2S_SD,
    .invert_flags =
    { .mclk_inv = false,
      .bclk_inv = false,
      .ws_inv = false,
    },
  },
};

/** @brief Global handle to the I2S RX channel configured with std_cfg. */
i2s_chan_handle_t  rx_handle;


bool flg_is_recording    = false;  /**< True while an I2S recording session is in progress. */
bool flg_I2S_initialized = false;  /**< True after I2S_Record_Init() succeeds; guards against double-init. */

// ------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initialise and enable the I2S RX channel for microphone recording.
 * @return true  on success.
 * @return false if the I2S channel could not be created or enabled.
 */
bool I2S_Record_Init();

/**
 * @brief Disable and delete the I2S RX channel, releasing its resources.
 * @return true  on success.
 * @return false if the channel was not initialised or could not be stopped.
 */
bool I2S_Record_De_Init();

#endif
