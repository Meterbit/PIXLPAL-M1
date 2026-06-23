/**
 * @file ledDriver.h
 * @brief Derived panel-geometry macros for the HUB75 audio spectrum visualiser.
 *
 * Computes bar-width and layout constants from the runtime matrix dimensions
 * (kMatrixWidth / kMatrixHeight) and the number of frequency bands configured
 * in AudioSpectVisual_Set_t. Include after audSpecSettings.h and mtb_audio.h.
 */
#ifndef LED_DRIVER_H
#define LED_DRIVER_H


#pragma once
#include <stdlib.h>
#include "mtb_audio.h"
/* There are several options to display the data from the FFT.
 * 1. Use a ledstrip like WS2812 or simular
 * 2. Use a Hub75 display
 * 3. Using both is possible but not recommended because of the required speed.
 */

#define AUD_VIS_UP   1  /**< Direction constant: bars grow upward. */
#define AUD_VIS_DOWN 0  /**< Direction constant: bars grow downward. */

/**
 * @brief Returns the upper frequency cut-off (Hz) for a given FFT bucket index.
 * @param iBucket  Zero-based bucket index.
 * @return Upper frequency boundary in Hz for that bucket.
 */
extern int BucketFrequency(int iBucket);

#define PANEL_WIDTH  (kMatrixWidth)   /**< Active panel width in pixels (mirrors kMatrixWidth). */
#define PANEL_HEIGHT (kMatrixHeight)  /**< Active panel height in pixels (mirrors kMatrixHeight). */
#define BAR_WIDTH    (kMatrixWidth / (audioSpecVisual_Set.noOfBands))   /**< Pixel width of each frequency bar column. */
#define TOP          (kMatrixHeight - 0)                                /**< Maximum bar height (top of the panel). */
#define NeededWidth  (BAR_WIDTH * audioSpecVisual_Set.noOfBands)        /**< Total pixel width consumed by all bars; may be less than PANEL_WIDTH. */
#define NUM_LEDS     (kMatrixWidth * kMatrixHeight)                     /**< Total pixel count on the panel. */

extern int loopcounter; /**< Global loop iteration counter, used by some patterns for animation timing. */

#endif
