#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_graphics.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleDrawShapeApp_Task_H = NULL;
void exampleDrawShapeApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *exampleDrawShapes_App = new Mtb_Applications_StatusBar(exampleDrawShapeApp_Task, &exampleDrawShapeApp_Task_H, "exampleDrawShapeApp", 4096);

void exampleDrawShapeApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Init(mtb_Status_Bar_Clock_Sv);
// End of App parameter initialization

mtb_Panel_Clear_Screen();

mtb_Panel_Draw_Pixel565(21, 36, WHITE); // Draw a single pixel
// Draw some shapes
// mtb_Panel_Draw_Rect(10, 10, 50, 10, WHITE);
mtb_Panel_Draw_Circle(21, 36, 19, GREEN_PANTONE);
// Draw a filled triangle
mtb_Panel_Fill_Triangle(64, 16, 34, 56, 94, 56, CYAN_PROCESS);

// mtb_Panel_Draw_VLine(120, 10, 60, WHITE); // Draw a vertical line
// mtb_Panel_Draw_HLine(10, 120, 50, WHITE); // Draw a horizontal line
// // // Draw a rounded rectangle
mtb_Panel_Draw_Round_Rect(94, 16, 30, 40, 5, PINK);
// // // Draw a filled rounded rectangle
// mtb_Panel_Fill_Round_Rect(97, 19, 24, 34, 5, ORANGE_PANTONE);
// Fill the screen with a fixed color color
// mtb_Panel_Fill_Screen(GREEN_LIZARD); 


while (MTB_APP_IS_ACTIVE == pdTRUE) {
ESP_LOGI("ExampleDrawShapesApp", "Shapes drawn on the display.");

delay(15000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(thisApp);
}
