#include <Arduino.h>
#include "mtb_engine.h"
#include "mtb_buzzer.h"

static const char TAG[] = "EX_ENCODER_BEEP_APP";

EXT_RAM_BSS_ATTR TaskHandle_t exampleEncoderBeepApp_Task_H = NULL;
void exampleEncoderBeepApp_Task(void *dApplication);

// button and encoder callback functions for the Rotary Encoder Control
void exampleAppButtonFn(button_event_t button_Data);
void exampleAppEncoderFn(rotary_encoder_rotation_t direction);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleEncoderBeep_App = new Mtb_Applications_FullScreen(exampleEncoderBeepApp_Task, &exampleEncoderBeepApp_Task_H, "exampleEncoderBeepApp");

void exampleEncoderBeepApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *THIS_APP = (Mtb_Applications *)dApplication;
  THIS_APP->mtb_App_Set_EC11_Cb_Fns(exampleAppButtonFn, exampleAppEncoderFn);
  THIS_APP->mtb_App_Init();
// End of App parameter initialization

// Declare Fixed and Scroll Text Variables
Mtb_FixedText_t exampleFixedText(2,30, Terminal6x8, GREEN);

// Write Fixed Text to display
exampleFixedText.mtb_Write_String("Encoder & Buzzer App");

mtb_Do_Beep(BEEP_0);

while (THIS_APP_IS_ACTIVE == pdTRUE) {
// Scroll the ScrollText Variable on display every 15 seconds
ESP_LOGI(TAG, "Use the Rotary Encoder to Rotate or Press the Button");
delay(15000);
}

// Clean up the application before exiting
  mtb_Delete_This_App(THIS_APP);
}

// ROTARY ENCODER CALLBACK FUNCTION
void exampleAppEncoderFn(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
    mtb_Do_Beep(CLICK_BEEP);
    mtb_Set_Status_RGB_LED(PURPLE);
    printf("Clockwise\n");
    } else if(direction == ROT_COUNTERCLOCKWISE){
    mtb_Do_Beep(CLICK_BEEP);
    mtb_Set_Status_RGB_LED(GREEN);
    printf("Counter Clockwise\n");
    }
}

// BUTTON CALLBACK FUNCTION
void exampleAppButtonFn(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            //mtb_Do_Beep(BEEP_0);
            break;

            case BUTTON_PRESSED:
            mtb_Do_Beep(BEEP_0);
            break;

            case BUTTON_PRESSED_LONG:
            mtb_Do_Beep(BEEP_1);
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
            mtb_Do_Beep(BEEP_2);
            break;
            case 3:
            mtb_Do_Beep(BEEP_3);
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}

