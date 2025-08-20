#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_graphics.h"
#include "exampleDrawShapesApp.h"


EXT_RAM_BSS_ATTR TaskHandle_t exampleDrawShapeApp_Task_H = NULL;

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *exampleDrawShapes_App = new Mtb_Applications_StatusBar(exampleDrawShapeApp_Task, &exampleDrawShapeApp_Task_H, "exampleDrawShapeApp", 4096);

void exampleDrawShapeApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
// End of App parameter initialization

dma_display->clearScreen();

dma_display->drawPixel(21, 36, WHITE); // Draw a single pixel
// Draw some shapes
// dma_display->drawLine(10, 10, 50, 10, WHITE);
dma_display->drawCircle(21, 36, 19, GREEN_PANTONE);
// // // Draw a filled triangle
dma_display->fillTriangle(64, 16, 34, 56, 94, 56, CYAN_PROCESS);

// dma_display->drawFastVLine(120, 10, 50, WHITE); // Draw a vertical line
// dma_display->drawFastHLine(10, 120, 50, WHITE); // Draw a horizontal line
// // // Draw a rounded rectangle
dma_display->drawRoundRect(94, 16, 30, 40, 5, PINK);
// // // Draw a filled rounded rectangle
dma_display->fillRoundRect(97, 19, 24, 34, 5, ORANGE_PANTONE);
// // // Draw a triangle with a different color
// dma_display->fillScreen(GREEN_LIZARD); // Clear the screen before drawing
// dma_display->write("The devil is in the details. ");

while (MTB_APP_IS_ACTIVE == pdTRUE) {
ESP_LOGI("ExampleDrawShapesApp", "Shapes drawn on the display.");

delay(15000);
}

// Clean up the application before exiting
  mtb_End_This_App(thisApp);
}
