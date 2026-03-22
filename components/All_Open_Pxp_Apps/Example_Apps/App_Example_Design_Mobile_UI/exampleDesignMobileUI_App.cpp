#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "exampleUI_Design_Components.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleDesignMobileUI_Task_H = NULL;
void exampleDesignMobileUI_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleDesignMobileUI_App = new Mtb_Applications_FullScreen(exampleDesignMobileUI_Task, &exampleDesignMobileUI_Task_H, "exampleDesignMobileUI_App", 4096);

void exampleDesignMobileUI_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Set_Mobile_UI(manifest, ui_api);
  thisApp->mtb_App_Set_EC11_Cb_Fns();
  thisApp->mtb_App_Init();
// End of App parameter initialization

// Declare Fixed and Scroll Text Variables
  Mtb_FixedText_t exampleFixedText(24,0, Terminal6x8, LEMON_CRAYOLA);

// Write Fixed Text to display
  exampleFixedText.mtb_Write_String("Dev Practice App"); // Write text in default color

  while (MTB_APP_IS_ACTIVE == pdTRUE) {
    delay(1000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(thisApp);
}


