#ifndef AUD_SPEC_SETTINGS
#define AUD_SPEC_SETTINGS

 /********************************************************************************************************************************************************
 *                                                                                                                                                       *
 *  Project:         FFT Spectrum Analyzer                                                                                                               *
 *  Target Platform: ESP32                                                                                                                               *
 *                                                                                                                                                       *
 *  Version: 1.0                                                                                                                                         *
 *  Hardware setup: See github                                                                                                                           *
 *  Spectrum analyses done with analog chips MSGEQ7                                                                                                      *
 *                                                                                                                                                       *
 *  Mark Donners                                                                                                                                         *
 *  The Electronic Engineer                                                                                                                              *
 *  Website:   www.theelectronicengineer.nl                                                                                                              *
 *  facebook:  https://www.facebook.com/TheelectronicEngineer                                                                                            *
 *  youtube:   https://www.youtube.com/channel/UCm5wy-2RoXGjG2F9wpDFF3w                                                                                  *
 *  github:    https://github.com/donnersm                                                                                                               *
 *                                                                                                                                                       *
 ********************************************************************************************************************************************************/

/**
 * @file audSpecSettings.h
 * @brief Compile-time tuning constants and runtime state variables for the FFT spectrum visualiser.
 *
 * Adapted from the FFT Spectrum Analyzer project by Mark Donners (The Electronic Engineer).
 * Include this header in exactly ONE translation unit — it defines global variables directly
 * (not just declarations) to avoid multiple-definition linker errors.
 */
#pragma once
#include <stdlib.h>
#include "arduinoFFT.h"
#include <pgmspace.h>

extern uint8_t kMatrixWidth;  /**< Panel pixel width, set at runtime from PANEL_RES_X. */
extern uint8_t kMatrixHeight; /**< Panel pixel height, set at runtime from PANEL_RES_Y. */

const int samplingFrequency = 16000; /**< ADC/I2S sampling frequency in Hz used for FFT computation. */
#define SAMPLEBLOCK  1024            /**< Number of samples per FFT frame. */

/** @defgroup aud_spec_options Visualiser Tuning Options
 *  @{
 */
#define BottomRowAlwaysOn   1   /**< Keep the bottom row of pixels always lit (HUB75 only). */
#define Fallingspeed        5   /**< Pixel-drop speed for falling bar tiles (higher = faster fall). */

int Peakdelay =             60;     /**< Frames to hold the peak dot before it begins falling. */
#define GAIN_DAMPEN         2       /**< Auto-gain reaction damping factor (higher = slower response). */
#define SecToChangePattern  10      /**< Seconds before the pattern auto-advances in random mode. */
#define MAX_VU              5000    /**< Maximum expected VU meter value (empirically tuned). */

int buttonPushCounter =     0;      /**< Starting pattern index after boot (0 – 22). */
bool autoChangePatterns =   true;   /**< When true, the pattern cycles automatically at SecToChangePattern intervals. */
int NoiseTresshold =        1500;   /**< FFT magnitude below this value is treated as silence (affects upper bands most). */
/** @} */

//buttonstuff — don't change unless you know what you are doing

/** @defgroup aud_spec_controls Hardware Control Pin Assignments
 *  @{
 */
#define BRIGHTNESSPOT 2   /**< ADC channel for the brightness potentiometer. */
#define PEAKDELAYPOT  4   /**< ADC channel for the peak-delay potentiometer. */
/** @} */

// Other stuff — don't change
#define ARRAYSIZE(a)    (sizeof(a)/sizeof(a[0]))  /**< Element count of a statically-sized array. */

char* PeakFlag  = nullptr; /**< Per-band peak-dot state flags (heap-allocated, length = noOfBands). Non-zero while the dot is floating. */
int*  PeakTimer = nullptr; /**< Per-band countdown timers (heap-allocated) controlling how long the peak dot floats before falling. */

// static char PeakFlag[64];
// int* PeakTimer[64];

volatile float gVU  = 0; /**< Instantaneous VU level computed from the current FFT frame. */
volatile float oldVU = 0; /**< VU level from the previous FFT frame, used for smoothing. */

double* vReal = nullptr; /**< Real component of the FFT input/output buffer (heap-allocated, SAMPLEBLOCK doubles). */
double* vImag = nullptr; /**< Imaginary component of the FFT input buffer (heap-allocated, SAMPLEBLOCK doubles). */
//int16_t samples[SAMPLEBLOCK];
arduinoFFT* FFT = new arduinoFFT(); /**< Global arduinoFFT instance used for spectrum computation. */
uint8_t* peak         = nullptr; /**< Per-band peak-dot height values (heap-allocated, length >= noOfBands). */
int*     oldBarHeights = nullptr; /**< Previous frame bar heights for smooth animation (heap-allocated, length >= noOfBands). */
float*   FreqBins      = nullptr; /**< Accumulated FFT energy per frequency band (heap-allocated, length >= noOfBands). */

// arduinoFFT* FFT = new arduinoFFT();
// uint8_t peak[65] = {0};
// int oldBarHeights[65] = {0};
// float FreqBins[65] = {0};

/** @defgroup aud_spec_tribar_colors TriBar Pattern Colours (HUB75 RGB)
 *  @brief RGB colour macros for the three-zone tri-colour bar pattern.
 *  @{
 */
#define TriBar_RGB_Top      255 , 0, 0    /**< Top zone colour (red). */
#define TriBar_RGB_Bottom   0 , 255, 0   /**< Bottom zone colour (green). */
#define TriBar_RGB_Middle   255, 255, 0  /**< Middle zone colour (yellow). */

#define TriBar_RGB_Top_Peak      255 , 0, 0   /**< Peak-dot colour in the top zone (red). */
#define TriBar_RGB_Bottom_Peak   0 , 255, 0   /**< Peak-dot colour in the bottom zone (green). */
#define TriBar_RGB_Middle_Peak   255, 255, 0  /**< Peak-dot colour in the middle zone (yellow). */
/** @} */

/** @defgroup aud_spec_center_colors CenterBars Pattern Colours (HUB75 RGB)
 *  @{
 */
#define Center_RGB_Edge      255 , 0, 0    /**< Edge pixel colour for the centre bar pattern (red). */
#define Center_RGB_Middle    255, 255, 0   /**< Interior pixel colour for the centre bar pattern (yellow). */

#define Center_RGB_Edge2     255 , 0, 0    /**< Edge pixel colour for the centre bar variant 2 (red). */
#define Center_RGB_Middle2   255, 255, 255 /**< Interior pixel colour for the centre bar variant 2 (white). */
/** @} */

#endif
