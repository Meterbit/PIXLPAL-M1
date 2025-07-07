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
 
#pragma once
// Depending on how many bands have been defined, one of these tables will contain the frequency
// cutoffs for that "size" of a spectrum display. 
// Only one of the following should be 1, rest has to be 0 
// WARNING amke sure your math add up.. if you select 24 bands on a 32 wide matrix....you'll need to adjust code
 enum PATTERN_T{
   PATTERN_0 = 0,
   PATTERN_1,
   PATTERN_2,
   PATTERN_3,
   PATTERN_4,
   PATTERN_5,
   PATTERN_6,
   PATTERN_7,
   PATTERN_8,
   PATTERN_9,
   PATTERN_10,
   PATTERN_11,
   PATTERN_12,
   PATTERN_13,
   PATTERN_14,
   PATTERN_15,
   PATTERN_16,
   PATTERN_17,
   PATTERN_18,
   PATTERN_19,
   PATTERN_20,
   PATTERN_21,
   PATTERN_22,
   PATTERN_23
 };


 static const int BandCutoffTable[64] = {
     45, 90, 130, 180, 220, 260, 310, 350, 390, 440, 480, 525, 565, 610, 650, 690, 735, 780, 820, 875, 920, 950, 1000, 1050, 1080, 1120, 1170, 1210, 1250, 1300, 1340, 1380, 1430, 1470, 1510, 1560, 1616, 1767, 1932, 2113, 2310, 2526, 2762, 3019, 3301, 3610, 3947, 4315, 4718, 5159, 5640, 6167, 6743, 7372, 8061, 8813, 9636, 10536, 11520, 12595, 13771, 15057, 16463, 18000};

