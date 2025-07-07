#ifndef LED_DRIVER_H
#define LED_DRIVER_H


#pragma once
#include <stdlib.h>
#include "mtbAudio.h"
/* There are several options to display the data from the FFT.
 * 1. Use a ledstrip like WS2812 or simular
 * 2. Use a Hub75 display
 * 3. Using both is possible but not recommended because of the required speed.
 */

#define AUD_VIS_UP 1
#define AUD_VIS_DOWN 0


extern int BucketFrequency(int iBucket);

#define PANEL_WIDTH  (kMatrixWidth)
#define PANEL_HEIGHT (kMatrixHeight)
#define BAR_WIDTH (kMatrixWidth / (audioSpecVisual_Set.noOfBands))   // If width >= 8 light 1 LED width per bar, >= 16 light 2 LEDs width bar etc
#define TOP (kMatrixHeight - 0)                 // Don't allow the bars to go offscreen
#define NeededWidth (BAR_WIDTH * audioSpecVisual_Set.noOfBands)      // we need this to see if all bands fit or that we have left over space
#define NUM_LEDS (kMatrixWidth * kMatrixHeight) // Total number of LEDs
extern int loopcounter;

#endif