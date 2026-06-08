#include "Arduino.h"
#include "mtb_engine.h"

static const char TAG[] = "PXP-Template_App";

EXT_RAM_BSS_ATTR TaskHandle_t pxp_Template_App_Task_H = NULL;
void pxp_Template_App_Task(void *dApplication);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *pxp_Template_App = new Mtb_Applications_StatusBar(pxp_Template_App_Task, &pxp_Template_App_Task_H, "pxp_Template_App");

void pxp_Template_App_Task(void* dApplication){
// ****** Initialize the App Parameters
    Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
    THIS_APP->mtb_App_Init();

    while (THIS_APP_IS_ACTIVE == pdTRUE){
        uint16_t countDown = 15000; // Countdown timer in milliseconds (15 seconds)
        // ****** App loop code goes here ******
        
        printf("\nThis is a print statement using the C api (printf)\n");

        Serial.println("\n This is a print statement using the Arduino API");

        ESP_LOGI(TAG, "\n This is a print statement using the ESP_LOG API (Information)\n");

        ESP_LOGW(TAG, "\n This is a print statement using the ESP_LOG API (Warning)\n");

        while(countDown --> 0 && THIS_APP_IS_ACTIVE == pdTRUE) delay(1); // Delay for 15 seconds before printing the message again
        // ****** End of App loop code ******
    }

    // ****** Clean up and delete the task when the app is no longer active ******
    printf("Exiting Template App...\n");

    mtb_Delete_This_App(THIS_APP);
}