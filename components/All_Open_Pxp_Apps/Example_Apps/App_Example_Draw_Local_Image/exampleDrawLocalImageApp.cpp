#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_graphics.h"

static const char TAG[] = "DRAW LOCAL IMAGE EXAMPLE APP";

EXT_RAM_BSS_ATTR TaskHandle_t exampleDrawLocalImageApp_Task_H = NULL;
void exampleDrawLocalImageApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleDrawLocalImages_App = new Mtb_Applications_FullScreen(exampleDrawLocalImageApp_Task, &exampleDrawLocalImageApp_Task_H, "exampleDrawImageApp");

void exampleDrawLocalImageApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Init();
// End of App parameter initialization

    // // DRAW PNG IMAGE FROM SPIFFS
    // mtb_Draw_Local_Png({"/appAssets/mechanic.png", 0, 0, 1});

    // DRAW SVG IMAGE FROM SPIFFS
    // mtb_Draw_Local_Svg({"/appAssets/spain.svg", 0, 0, 1});

    // DRAW GIF ANIMATION FROM SPIFFS
    mtb_Draw_Local_Gif({"/clkgif/clk00.gif", 20, 10, 5});

while (THIS_APP_IS_ACTIVE == pdTRUE) {
ESP_LOGI(TAG, "Local Images drawn on the display.");
delay(5000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(THIS_APP);
}
