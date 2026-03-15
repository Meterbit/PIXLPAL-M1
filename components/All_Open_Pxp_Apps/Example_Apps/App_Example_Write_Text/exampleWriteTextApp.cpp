#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "exampleWriteTextApp.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleWriteTextApp_Task_H = NULL;

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleWriteText_App = new Mtb_Applications_FullScreen(exampleWriteTextApp_Task, &exampleWriteTextApp_Task_H, "exampleWriteTextApp", 4096);

void exampleWriteTextApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Init();
// End of App parameter initialization

// Declare Fixed and Scroll Text Variables
  Mtb_FixedText_t exampleFixedText(24,15, Terminal8x12, GREEN);
  Mtb_ScrollText_t exampleScrollText (5, 40, 118, Terminal6x8, WHITE, 20, 1);

// Write Fixed Text to display
  exampleFixedText.mtb_Write_String("Hello World."); // Write text in default color
//exampleFixedText.mtb_Write_Colored_Text(" in Color!", PURPLE);     // Write text in different color

  while (MTB_APP_IS_ACTIVE == pdTRUE) {

// Scroll the ScrollText Variable on display every 15 seconds
  exampleScrollText.mtb_Scroll_This_Text("PIXLPAL - A project by Meterbit Cybernetics");      // Scroll text in default color
  //exampleScrollText.mtb_Scroll_This_Text("Visit us at www.meterbitcyb.com", CYAN);          // Scroll text in different color
  
  delay(15000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(thisApp);
}


