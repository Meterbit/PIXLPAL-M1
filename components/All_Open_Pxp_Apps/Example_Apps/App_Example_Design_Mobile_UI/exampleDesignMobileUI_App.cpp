#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "exampleUI_Design_Components.h"

EXT_RAM_BSS_ATTR TaskHandle_t exampleDesignMobileUI_Task_H = NULL;
void exampleDesignMobileUI_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleDesignMobileUI_App;
Mtb_Applications* exampleDesignMobileUI_App_GetInstance() {
    if (!exampleDesignMobileUI_App) exampleDesignMobileUI_App = new Mtb_Applications_FullScreen(exampleDesignMobileUI_Task, &exampleDesignMobileUI_Task_H, "exampleDesignMobileUI_App", {11,0}, 4096);
    return exampleDesignMobileUI_App;
}
MTB_REGISTER_APP(exampleDesignMobileUI_App, 11, 0)

void exampleDesignMobileUI_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_Mobile_UI(manifest, ui_api);
  THIS_APP->mtb_App_Set_EC11_Cb_Fns();
  THIS_APP->mtb_App_Init();
// End of App parameter initialization

// Declare Fixed and Scroll Text Variables
  Mtb_FixedText_t exampleFixedText(24,0, Terminal6x8, LEMON_CRAYOLA);

// Write Fixed Text to display
  exampleFixedText.mtb_Write_String("Dev Practice App"); // Write text in default color

  // Declare Variable for monitoring Free/Available internal SRAM
  size_t free_sram = 0;

  while (THIS_APP_IS_ACTIVE == pdTRUE) {
    // // Get the total free size of internal SRAM
    // free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // // Print the free SRAM size to the console.
    // printf("############ Free Internal SRAM: %zu bytes\n", free_sram);

    // // delay 5 seconds
    delay(5000);
  }

// Clean up the application before exiting
  mtb_Delete_This_App(THIS_APP);
}


