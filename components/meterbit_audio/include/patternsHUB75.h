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
 * @file patternsHUB75.h
 * @brief HUB75 audio spectrum visualiser bar and peak-dot pattern drawing functions.
 *
 * Adapted from the FFT Spectrum Analyzer project by Mark Donners (The Electronic Engineer).
 * Each bar pattern function draws one frequency band column at the computed bar height.
 * Each peak function draws a single peak-dot above the bar for that band.
 * This is a header-only implementation file — include it in exactly ONE translation unit.
 *
 * All functions write directly to the HUB75 panel via mtb_Panel_Draw_PixelRGB().
 * Layout macros (BAR_WIDTH, TOP, NeededWidth) are defined in ledDriver.h and depend
 * on the runtime values of kMatrixWidth, kMatrixHeight, and audioSpecVisual_Set.noOfBands.
 */

#pragma once

#include "mtb_graphics.h"
#include "audSpecSettings.h"
#include "mtb_nvs.h"
#include "Arduino.h"

// Define the map function if it's not available
/**
 * @brief Linear map of a value from one integer range to another.
 * @param x       Input value to map.
 * @param in_min  Lower bound of the input range.
 * @param in_max  Upper bound of the input range.
 * @param out_min Lower bound of the output range.
 * @param out_max Upper bound of the output range.
 * @return Mapped output value.
 */
long mappers(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
 * First all the bar patterns
 */

/**
 * @brief Draw a per-band-hued colour bar for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels from the bottom of the panel.
 */
void ColorBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){

        mtb_Panel_Draw_PixelRGB(x,y,(band+1)*40,(band+1)*30,255-((band+1)*70));      //middle
      //   mtb_Panel_Draw_PixelRGB(x,y,band*40,band*30,150-(band*10));      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw a solid red bar for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void RedBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;


  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){

        mtb_Panel_Draw_PixelRGB(x,y,250,0,0);      //middle
      //   mtb_Panel_Draw_PixelRGB(x,y,band*40,band*30,150-(band*10));      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw alternating red/yellow bars for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void Twins(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;


  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){
        if((band & 1)==1)mtb_Panel_Draw_PixelRGB(x,y,250,0,0);
        else mtb_Panel_Draw_PixelRGB(x,y,250,250,0);      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw alternating magenta/cyan bars for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void Twins2(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;


  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){
        if((band & 1)==1)mtb_Panel_Draw_PixelRGB(x,y,250,0,250);
        else mtb_Panel_Draw_PixelRGB(x,y,0,250,250);      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw a three-zone (red / yellow / green) colour bar for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void TriBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;


  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){

        if (y < (PANEL_HEIGHT/4)) mtb_Panel_Draw_PixelRGB(x, y,TriBar_RGB_Top );     //Top
        else if (y > (PANEL_HEIGHT/2)) mtb_Panel_Draw_PixelRGB(x, y, TriBar_RGB_Bottom ); // bottom
        else  mtb_Panel_Draw_PixelRGB(x,y,TriBar_RGB_Middle);      //middle
        //else  mtb_Panel_Draw_PixelRGB(x,y,TriBar_Color_Middle_RGB);      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw a blue-filled bar with a red outline box for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void BoxedBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
     if(y >= TOP - barHeight){

      if (y==(TOP - barHeight))mtb_Panel_Draw_PixelRGB(x,y,250,0,0);
      else if (x==xStart)mtb_Panel_Draw_PixelRGB(x,y,250,0,0); // Border left side of the bars
      else if(x==xStart+BAR_WIDTH-1)mtb_Panel_Draw_PixelRGB(x,y,250,0,0); // Border right side of the bars
      else mtb_Panel_Draw_PixelRGB(x,y,0,0,250);

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }
}

/**
 * @brief Draw a blue-filled bar with a white outline box for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void BoxedBars2(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
     if(y >= TOP - barHeight){

      if (y==(TOP - barHeight))mtb_Panel_Draw_PixelRGB(x,y,250,250,250);
      else if (x==xStart)mtb_Panel_Draw_PixelRGB(x,y,250,250,250); // Border left side of the bars
      else if(x==xStart+BAR_WIDTH-1)mtb_Panel_Draw_PixelRGB(x,y,250,250,250); // Border right side of the bars
      else mtb_Panel_Draw_PixelRGB(x,y,0,0,250);

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/**
 * @brief Draw a yellow-filled bar with a green outline box for the given frequency band.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void BoxedBars3(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
     if(y >= TOP - barHeight){

      if (y==(TOP - barHeight))mtb_Panel_Draw_PixelRGB(x,y,0,255,0);
      else if (x==xStart)mtb_Panel_Draw_PixelRGB(x,y,0,255,0); // Border left side of the bars
      else if(x==xStart+BAR_WIDTH-1)mtb_Panel_Draw_PixelRGB(x,y,0,255,0); // Border right side of the bars
      else mtb_Panel_Draw_PixelRGB(x,y,200,200,0);

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }
}

/**
 * @brief Draw a centre-expanding bar (growing outward from the vertical midpoint) in yellow/red.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Total bar height in pixels (split equally above and below centre).
 */
void centerBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int center = TOP/2;

  if (barHeight>(kMatrixHeight-6))barHeight=kMatrixHeight-6;
 // ESP_LOGI(TAG,  "barheight: %d \n",barHeight);
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = 0; y <= (barHeight/2); y++) {
     // if(y >= TOP - barHeight){

    if(y==(barHeight/2)){
          mtb_Panel_Draw_PixelRGB(x,center+y,Center_RGB_Edge);      //edge
         mtb_Panel_Draw_PixelRGB(x,center-y-1,Center_RGB_Edge);      //edge
      }
    else  {
    mtb_Panel_Draw_PixelRGB(x,center+y,Center_RGB_Middle);      //middle
    mtb_Panel_Draw_PixelRGB(x,center-y-1,Center_RGB_Middle);      //middle
    }
     }
     for (int y= barHeight/2;y<TOP;y++){
      mtb_Panel_Draw_PixelRGB(x, center+y+1, 0, 0, 0); // make unused pixel bottom black
      if((center-y-2)>1)mtb_Panel_Draw_PixelRGB(x, center-y-2, 0, 0, 0); // make unused pixel top black except those of the VU meter
     }


  }

}

/**
 * @brief Draw a centre-expanding bar (variant 2) in white/red.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Total bar height in pixels.
 */
void centerBars2(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int center = TOP/2;

  if (barHeight>(kMatrixHeight-6))barHeight=kMatrixHeight-6;
 // ESP_LOGI(TAG,  "barheight: %d \n",barHeight);
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = 0; y <= (barHeight/2); y++) {
     // if(y >= TOP - barHeight){

    if(y==(barHeight/2)){
          mtb_Panel_Draw_PixelRGB(x,center+y,Center_RGB_Edge2);      //edge
         mtb_Panel_Draw_PixelRGB(x,center-y-1,Center_RGB_Edge2);      //edge
      }
    else  {
    mtb_Panel_Draw_PixelRGB(x,center+y,Center_RGB_Middle2);      //middle
    mtb_Panel_Draw_PixelRGB(x,center-y-1,Center_RGB_Middle2);      //middle
    }
     }
     for (int y= barHeight/2;y<TOP;y++){
      mtb_Panel_Draw_PixelRGB(x, center+y+1, 0, 0, 0); // make unused pixel bottom black
      if((center-y-2)>1)mtb_Panel_Draw_PixelRGB(x, center-y-2, 0, 0, 0); // make unused pixel top black except those of the VU meter
     }


  }

}

/**
 * @brief Erase the column for the given frequency band (all pixels black).
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Unused; present for interface consistency with other bar functions.
 */
void BlackBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {

  for (int y = TOP; y >= 2; y--) {
      if(y >= TOP - barHeight){

        mtb_Panel_Draw_PixelRGB(x,y,0,0,0);      //middle

     }
      else {
       // leds[i].fadeToBlackBy( 64 );

      mtb_Panel_Draw_PixelRGB(x,y,0,0,0);

      }
    }
  }

}

/*
 * All the Peak Patterns
 */

/**
 * @brief Draw a red peak dot at the current peak height for the given frequency band.
 * @param band  Frequency band index (0 – noOfBands-1).
 */
void RedPeak(int band) {
 // #ifdef Ledstrip
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int peakHeight = TOP - peak[band] - 1;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
   // matrix->drawPixel(x, peakHeight, CHSV(0,255,0));
   mtb_Panel_Draw_PixelRGB(x,peakHeight,255,0,0);
     }
 // #endif
}

/**
 * @brief Draw a white peak dot at the current peak height for the given frequency band.
 * @param band  Frequency band index (0 – noOfBands-1).
 */
void WhitePeak(int band) {
 // #ifdef Ledstrip
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int peakHeight = TOP - peak[band] - 1;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
   // matrix->drawPixel(x, peakHeight, CHSV(0,255,0));
   mtb_Panel_Draw_PixelRGB(x,peakHeight,255,255,255);
     }
 // #endif
}

/**
 * @brief Draw a blue peak dot at the current peak height for the given frequency band.
 * @param band  Frequency band index (0 – noOfBands-1).
 */
void BluePeak(int band) {
 // #ifdef Ledstrip
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int peakHeight = TOP - peak[band] - 1;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
   // matrix->drawPixel(x, peakHeight, CHSV(0,255,0));
   mtb_Panel_Draw_PixelRGB(x,peakHeight,0,0,255);
     }
 // #endif
}

/**
 * @brief Draw a two-pixel-tall blue peak dot at the current peak height for the given band.
 * @param band  Frequency band index (0 – noOfBands-1).
 */
void DoublePeak(int band) {

  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int peakHeight = TOP - peak[band] - 1;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
   // matrix->drawPixel(x, peakHeight, CHSV(0,255,0));
   mtb_Panel_Draw_PixelRGB(x,peakHeight,0,0,255);
   mtb_Panel_Draw_PixelRGB(x,peakHeight+1,0,0,255);
     }

}

/**
 * @brief Draw a three-zone (red / yellow / green) coloured peak dot matching the TriBar zones.
 * @param band  Frequency band index (0 – noOfBands-1).
 */
void TriPeak(int band) {
  int xStart = BAR_WIDTH * band;
  if(NeededWidth<kMatrixWidth)xStart+= (kMatrixWidth-NeededWidth)/2;
  int peakHeight = TOP - peak[band] - 1;
  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {


  if (peakHeight < (PANEL_HEIGHT/4)) mtb_Panel_Draw_PixelRGB(x,peakHeight,TriBar_RGB_Top); //Top red
    else if (peakHeight > (PANEL_HEIGHT/2)) mtb_Panel_Draw_PixelRGB(x,peakHeight,TriBar_RGB_Bottom); //green
    else mtb_Panel_Draw_PixelRGB(x,peakHeight,TriBar_RGB_Middle); //yellow

  }
}

//####################################################################################################################
//####################################################################################################################

/**
 * @brief Draw a vertical gradient bar shading from blue (bottom) to yellow (top).
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void GradientBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        int colorValue = mappers(y, TOP - barHeight, TOP, 0, 255);
        mtb_Panel_Draw_PixelRGB(x, y, colorValue, colorValue, 255 - colorValue);
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a checkerboard-patterned bar alternating black and white pixels.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void CheckerboardBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if (((x + y) % 2) == 0) {
          mtb_Panel_Draw_PixelRGB(x, y, 255, 255, 255);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a rainbow-hued gradient bar cycling through hue with height.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void RainbowGradientBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        int hue = mappers(y, TOP - barHeight, TOP, 0, 255);
        mtb_Panel_Draw_PixelRGB(x, y, hue, 255, 255 - hue);
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar with alternating two-pixel-tall green and blue horizontal stripes.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void StripedBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if ((y / 2) % 2 == 0) {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 255, 0);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 255);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar with diagonal red/blue stripes at a 45-degree angle.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void DiagonalBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if ((x + y) % 10 < 5) {
          mtb_Panel_Draw_PixelRGB(x, y, 255, 0, 0);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 255);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar with a horizontal colour gradient that varies across the bar width.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void VerticalGradientBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        int colorValue = mappers(x, xStart, xStart + BAR_WIDTH - 1, 0, 255);
        mtb_Panel_Draw_PixelRGB(x, y, colorValue, 255 - colorValue, colorValue / 2);
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar with an orange/purple zigzag tile pattern.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void ZigzagBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if (((x + y) / 5) % 2 == 0) {
          mtb_Panel_Draw_PixelRGB(x, y, 255, 100, 0);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 100, 0, 255);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a sparse dot bar where only even-coordinate pixels are lit in cyan.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void DottedBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if ((x % 2 == 0) && (y % 2 == 0)) {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 255, 255);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar that fades from purple (short) to pink (tall) with height.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void ColorFadeBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        int colorValue = mappers(y, TOP - barHeight, TOP, 0, 255);
        mtb_Panel_Draw_PixelRGB(x, y, colorValue, 50, 255 - colorValue);
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar whose colour pulses through a blue-to-red cycle driven by millis().
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void PulsingBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;
  uint8_t pulse = (millis() / 10) % 256;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        mtb_Panel_Draw_PixelRGB(x, y, pulse, 0, 255 - pulse);
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}

/**
 * @brief Draw a bar that alternates between red and green every 500 ms.
 * @param band       Frequency band index (0 – noOfBands-1).
 * @param barHeight  Bar height in pixels.
 */
void FlashingBars(int band, int barHeight) {
  int xStart = BAR_WIDTH * band;
  if (NeededWidth < kMatrixWidth) xStart += (kMatrixWidth - NeededWidth) / 2;
  bool flash = (millis() / 500) % 2;

  for (int x = xStart; x < xStart + BAR_WIDTH; x++) {
    for (int y = TOP; y >= 2; y--) {
      if (y >= TOP - barHeight) {
        if (flash) {
          mtb_Panel_Draw_PixelRGB(x, y, 255, 0, 0);
        } else {
          mtb_Panel_Draw_PixelRGB(x, y, 0, 255, 0);
        }
      } else {
        mtb_Panel_Draw_PixelRGB(x, y, 0, 0, 0);
      }
    }
  }
}
