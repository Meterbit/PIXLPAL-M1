


#ifndef MICROPHONE_H
#define MICROPHONE_H

#include "driver/i2s_std.h"     // instead of older legacy #include <driver/i2s.h>  

// --- defines & macros --------

#ifndef DEBUG                   // user can define favorite behaviour ('true' displays addition info)
#  define DEBUG true            // <- define your preference here  
#  define DebugPrint(x);        if(DEBUG){Serial.print(x);}   /* do not touch */
#  define DebugPrintln(x);      if(DEBUG){Serial.println(x);} /* do not touch */ 
#endif


// --- PIN assignments ---------

#define I2S_WS      47          // add-on: L/R pin INMP441 on Vcc is RIGHT channel, connected to GND is LEFT channel
#define I2S_SD      14          
#define I2S_SCK     21     


// --- define your settings ----

#define SAMPLE_RATE             16000  // typical values: 8000 .. 44100, use e.g 8K (and 8 bit mono) for smallest .wav files  
                                       // hint: best quality with 16000 or 24000 (above 24000: random dropouts and distortions)
                                       // recommendation in case the STT service produces lot of wrong words: try 16000 

#define BITS_PER_SAMPLE         16      // 16 bit and 8bit supported (24 or 32 bits not supported)
                                       // hint: 8bit is less critical for STT services than a low 8kHz sample rate
                                       // for fastest STT: combine 8kHz and 8 bit. 

#define GAIN_BOOSTER_I2S        32     // original I2S streams is VERY silent, so we added an optional GAIN booster for INMP441
                                       // multiplier, values: 1-64 (32 seems best value for INMP441)
                                       // 64: high background noise but working well for STT on quiet human conversations



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

// [re_handle]: global handle to the RX channel with channel configuration [std_cfg]
i2s_chan_handle_t  rx_handle;





bool flg_is_recording = false;         // only internally used

bool flg_I2S_initialized = false;      // to avoid any runtime errors in case user forgot to initialize

// ------------------------------------------------------------------------------------------------------------------------------

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

  printf("Microphone I2S channel enabled and created.\n");

  return flg_I2S_initialized;  
}

bool I2S_Record_De_Init(){
  i2s_channel_disable(rx_handle);
  i2s_del_channel(rx_handle);
  printf("Microphone I2S channel disabled and deleted.\n");
  return true;  
}

#endif