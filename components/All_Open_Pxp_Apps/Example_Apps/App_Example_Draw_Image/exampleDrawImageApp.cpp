#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_graphics.h"

static const char TAG[] = "DRAW IMAGE EXAMPLE APP";

EXT_RAM_BSS_ATTR TaskHandle_t exampleDrawImageApp_Task_H = NULL;
void exampleDrawImageApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleDrawImages_App = new Mtb_Applications_FullScreen(exampleDrawImageApp_Task, &exampleDrawImageApp_Task_H, "exampleDrawImageApp", 4096);

void exampleDrawImageApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Init();
// End of App parameter initialization

    while ((Mtb_Applications::internetConnectStatus != true) && (MTB_APP_IS_ACTIVE == pdTRUE)){
      ESP_LOGI(TAG, "Waiting for internet connection...");
      delay(1000);
    }

    // DRAW GIF ANIMATION FROM SPIFFS
    //mtb_Draw_Local_Gif({"/clkgif/clk00.gif", 0, 0, 10});

    // DRAW PNG IMAGE FROM SPIFFS
    // mtb_Draw_Local_Png({"/batIcons/fmRadio.png", 25, 10});

    // DRAW SVG IMAGE FROM SPIFFS
    mtb_Draw_Local_Svg({"/batIcons/spain.svg", 0, 0, 1});

while (MTB_APP_IS_ACTIVE == pdTRUE) {
ESP_LOGI(TAG, "Images drawn on the display.");

delay(5000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(thisApp);
}
