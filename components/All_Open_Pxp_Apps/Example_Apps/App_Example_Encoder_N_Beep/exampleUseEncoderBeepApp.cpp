#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_buzzer.h"
#include "exampleEncoderBeepApp.h"

static const char TAG[] = "EX_ENCODER_BEEP_APP";

EXT_RAM_BSS_ATTR TaskHandle_t exampleEncoderBeepApp_Task_H = NULL;

// button and encoder callback functions for the Rotary Encoder Control
void exampleAppButtonFn(button_event_t button_Data);
void exampleAppEncoderFn(rotary_encoder_rotation_t direction);

EXT_RAM_BSS_ATTR Mtb_Applications_StatusBar *exampleEncoderBeep_App = new Mtb_Applications_StatusBar(exampleEncoderBeepApp_Task, &exampleEncoderBeepApp_Task_H, "exampleEncoderBeepApp", 4096);

void exampleEncoderBeepApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_Set_EC11_Cb_Fns(exampleAppButtonFn, exampleAppEncoderFn);
  mtb_App_Init(thisApp, mtb_Status_Bar_Clock_Sv);
// End of App parameter initialization

// Declare Fixed and Scroll Text Variables
Mtb_FixedText_t exampleFixedText(2,30, Terminal6x8, GREEN);

// Write Fixed Text to display
exampleFixedText.mtb_Write_String("Encoder and Buzzer App");

while (MTB_APP_IS_ACTIVE == pdTRUE) {
// Scroll the ScrollText Variable on display every 15 seconds
ESP_LOGI(TAG, "Use the Rotary Encoder to Rotate or Press the Button");
delay(5000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(thisApp);
}

// ROTARY ENCODER CALLBACK FUNCTION
void exampleAppEncoderFn(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
    do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
    do_beep(CLICK_BEEP);
    }
}


// BUTTON CALLBACK FUNCTION
void exampleAppButtonFn(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            //do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED:
            do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED_LONG:
            do_beep(BEEP_1);
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
            do_beep(BEEP_2);
                break;
            case 3:
            do_beep(BEEP_3);
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}

