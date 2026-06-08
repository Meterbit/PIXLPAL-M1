
#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_graphics.h"

static const char TAG[] = "DRAW ONLINE IMAGE EXAMPLE APP";

EXT_RAM_BSS_ATTR TaskHandle_t exampleDrawOnlineImageApp_Task_H = NULL;
void exampleDrawOnlineImageApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleDrawOnlineImages_App = new Mtb_Applications_FullScreen(exampleDrawOnlineImageApp_Task, &exampleDrawOnlineImageApp_Task_H, "exampleDrawOnlineImageApp");

void exampleDrawOnlineImageApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Init();
// End of App parameter initialization

  while(Mtb_Applications::internetConnectStatus != pdTRUE){
    ESP_LOGI(TAG, "Waiting for Internet Connection to draw online images...");
    delay(1000);
  } 

  // To draw a single online image, create an Mtb_OnlineImage_t variable with the respective image link and drawing parameters, then call mtb_Draw_Online_Png() or mtb_Draw_Online_Svg() with the variable as the argument.
    Mtb_OnlineImage_t worldCountryFlag{
      "https://raw.githubusercontent.com/luukhopman/football-logos/master/logos/England%20-%20Premier%20League/Manchester%20City.png",
      0,
      0,
      3
    };

mtb_Draw_Online_Png(&worldCountryFlag);


  // To draw multiple online images, create an array of Mtb_OnlineImage_t with the respective image links and drawing parameters, then call mtb_Download_Multi_Svg() and mtb_Draw_Multi_Svg() with the count of images in the array.
  // Mtb_OnlineImage_t onlineSvgImages[2] = {
  //   {"https://raw.githubusercontent.com/joielechong/iso-country-flags-svg-collection/master/svg/country-squared/aw.svg", 5, 10, 3},
  //   {"https://raw.githubusercontent.com/joielechong/iso-country-flags-svg-collection/master/svg/country-squared/jp.svg", 70, 10, 3}
  // };

  // mtb_Download_Multi_Svg(onlineSvgImages, 2);
  // mtb_Draw_Multi_Svg(2);

while (THIS_APP_IS_ACTIVE == pdTRUE) {
ESP_LOGI(TAG, "Local Online Images drawn on the display.\n");
delay(5000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(THIS_APP);
}
