#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleWriteTextApp_Task_H = NULL;
void exampleWriteTextApp_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_FixedText_t* exampleFixedText = nullptr;

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleWriteText_App = new Mtb_Applications_FullScreen(exampleWriteTextApp_Task, &exampleWriteTextApp_Task_H, "exampleWriteTextApp");

void exampleWriteTextApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Init();
// End of App parameter initialization

// *****************************************************************************************************************************

  exampleFixedText = new Mtb_FixedText_t(5, 5, Terminal6x8, WHITE, PURPLE);
// Declare Fixed and Scroll Text Variables
  
  // Mtb_ScrollText_t exampleScrollText(5, 40, 118, Terminal6x8, WHITE, 20, 1);
  // Mtb_CentreText_t exampleCentreText(64, 25, Terminal8x12, YELLOW);

// Write Fixed Text to display
  exampleFixedText->mtb_Write_String("Sweet Field."); // Write text in default color
  delay(5000);
  exampleFixedText->mtb_Write_String("Hello World.");
  delay(5000);
  exampleFixedText->mtb_Write_Colored_String("Welcome 2 Pixlpal!", CYAN, YELLOW); // Write text in different color
  // exampleCentreText.mtb_Write_String("Welcome!"); // Write text in default color
  while (THIS_APP_IS_ACTIVE == pdTRUE) {

// Scroll the ScrollText Variable on display every 15 seconds
  // exampleScrollText.mtb_Scroll_This_Text("PIXLPAL - A project by Meterbit Cybernetics");      // Scroll text in default color
  // exampleScrollText.mtb_Scroll_This_Text("Visit us at www.meterbitcyb.com", CYAN);          // Scroll text in different color
  
  delay(15000);
}

  delete exampleFixedText;

  mtb_Delete_This_App(THIS_APP);
}


