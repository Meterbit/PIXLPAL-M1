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
 #include <stdlib.h>

/**
 * @file FFT.h
 * @brief FFT band enumeration and frequency cut-off table for the audio spectrum visualiser.
 *
 * Adapted from the FFT Spectrum Analyzer project by Mark Donners (The Electronic Engineer).
 * Defines PATTERN_T for selecting the active visualiser pattern and BandCutoffTable which
 * maps band indices to upper frequency cut-off values (Hz) for up to 64 frequency bands.
 */
#pragma once

/**
 * @brief Selectable audio spectrum visualiser pattern indices.
 *
 * Each enumerator corresponds to one of the pattern drawing functions defined in
 * patternsHUB75.h. The active pattern is stored in AudioSpectVisual_Set_t::selectedPattern.
 */
 enum PATTERN_T{
   PATTERN_0 = 0, /**< Pattern index 0  – ColorBars. */
   PATTERN_1,     /**< Pattern index 1  – RedBars. */
   PATTERN_2,     /**< Pattern index 2  – Twins. */
   PATTERN_3,     /**< Pattern index 3  – Twins2. */
   PATTERN_4,     /**< Pattern index 4  – TriBars. */
   PATTERN_5,     /**< Pattern index 5  – BoxedBars. */
   PATTERN_6,     /**< Pattern index 6  – BoxedBars2. */
   PATTERN_7,     /**< Pattern index 7  – BoxedBars3. */
   PATTERN_8,     /**< Pattern index 8  – centerBars. */
   PATTERN_9,     /**< Pattern index 9  – centerBars2. */
   PATTERN_10,    /**< Pattern index 10 – BlackBars. */
   PATTERN_11,    /**< Pattern index 11 – GradientBars. */
   PATTERN_12,    /**< Pattern index 12 – CheckerboardBars. */
   PATTERN_13,    /**< Pattern index 13 – RainbowGradientBars. */
   PATTERN_14,    /**< Pattern index 14 – StripedBars. */
   PATTERN_15,    /**< Pattern index 15 – DiagonalBars. */
   PATTERN_16,    /**< Pattern index 16 – VerticalGradientBars. */
   PATTERN_17,    /**< Pattern index 17 – ZigzagBars. */
   PATTERN_18,    /**< Pattern index 18 – DottedBars. */
   PATTERN_19,    /**< Pattern index 19 – ColorFadeBars. */
   PATTERN_20,    /**< Pattern index 20 – PulsingBars. */
   PATTERN_21,    /**< Pattern index 21 – FlashingBars. */
   PATTERN_22,    /**< Pattern index 22 – reserved. */
   PATTERN_23     /**< Pattern index 23 – reserved. */
 };

/**
 * @brief Upper frequency cut-off values in Hz for each of the 64 possible frequency bands.
 *
 * Index @c i gives the upper frequency boundary (Hz) of display band @c i. The FFT
 * processing assigns each FFT bin to a band by comparing bin frequency against these
 * thresholds. Only the first @c noOfBands entries are used at runtime.
 */
 static const int BandCutoffTable[64] = {
     45, 90, 130, 180, 220, 260, 310, 350, 390, 440, 480, 525, 565, 610, 650, 690, 735, 780, 820, 875, 920, 950, 1000, 1050, 1080, 1120, 1170, 1210, 1250, 1300, 1340, 1380, 1430, 1470, 1510, 1560, 1616, 1767, 1932, 2113, 2310, 2526, 2762, 3019, 3301, 3610, 3947, 4315, 4718, 5159, 5640, 6167, 6743, 7372, 8061, 8813, 9636, 10536, 11520, 12595, 13771, 15057, 16463, 18000};
