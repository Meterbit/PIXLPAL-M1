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
#pragma once
#include <stdlib.h>
#include "arduinoFFT.h"
#include <pgmspace.h>

extern uint8_t kMatrixWidth;
extern uint8_t kMatrixHeight;

const int samplingFrequency = 16000;
#define SAMPLEBLOCK  1024

//Options Change to your likings
#define BottomRowAlwaysOn   1                       // if set to 1, bottom row is always on. Setting only applies to LEDstrip not HUB75
#define Fallingspeed        5                       // Falling down factor that effects the speed of falling tiles

int Peakdelay =             60;                     // Delay before peak falls down to stack. Overruled by PEAKDEALY Potmeter
#define GAIN_DAMPEN         2                       // Higher values cause auto gain to react more slowly
#define SecToChangePattern  10                      // number of seconds that pattern changes when auto change mode is enabled
#define MAX_VU              5000                    // How high our VU could max out at.  Arbitarily tuned.
int buttonPushCounter =     0;                      // This number defines what pattern to start after boot (0 to 12)
bool autoChangePatterns =   true;                  // After boot, the pattern will not change automatically. 
int NoiseTresshold =        1500;                   // this will effect the upper bands most.
//buttonstuf don't change unless you know what you are doing

//Controls  //don't change unless you are using your own hardware design
#define BRIGHTNESSPOT 2 
#define PEAKDELAYPOT  4

// Other stuff don't change
#define ARRAYSIZE(a)    (sizeof(a)/sizeof(a[0]))
char* PeakFlag = nullptr;                           // the top peak delay needs to have a flag because it has different timing while floating compared to falling to the stack
int* PeakTimer = nullptr;                           // counter how many loops to stay floating before falling to stack

// static char PeakFlag[64];                           // the top peak delay needs to have a flag because it has different timing while floating compared to falling to the stack
// int* PeakTimer[64];                           // counter how many loops to stay floating before falling to stack

volatile float         gVU       = 0;              // Instantaneous read of VU value
volatile float         oldVU     = 0;              // Previous read of VU value

double* vReal = nullptr;
double* vImag = nullptr;
//int16_t samples[SAMPLEBLOCK];
arduinoFFT* FFT = new arduinoFFT(); /* Create FFT object */
uint8_t* peak = nullptr;              // The length of these arrays must be >= NUM_BANDS
int* oldBarHeights = nullptr;      // so they are set to 65
float* FreqBins = nullptr;

// arduinoFFT* FFT = new arduinoFFT(); /* Create FFT object */
// uint8_t peak[65] = {0};              // The length of these arrays must be >= NUM_BANDS
// int oldBarHeights[65] = {0};      // so they are set to 65
// float FreqBins[65] = {0};

// Colors mode 1 These are the colors from the TRIBAR when using HUB75
#define TriBar_RGB_Top      255 , 0, 0    // Red CRGB
#define TriBar_RGB_Bottom   0 , 255, 0   // Green CRGB
#define TriBar_RGB_Middle   255, 255, 0    // Yellow CRGB

#define TriBar_RGB_Top_Peak      255 , 0, 0    // Red CRGB
#define TriBar_RGB_Bottom_Peak   0 , 255, 0   // Green CRGB
#define TriBar_RGB_Middle_Peak   255, 255, 0    // Yellow CRGB

// hub 75 center bars
#define Center_RGB_Edge      255 , 0, 0    // Red CRGB
#define Center_RGB_Middle   255, 255, 0    // Yellow CRGB
// hub 75 center bars 2
#define Center_RGB_Edge2      255 , 0, 0    // Red CRGB
#define Center_RGB_Middle2   255, 255, 255    // Yellow CRGB

#endif