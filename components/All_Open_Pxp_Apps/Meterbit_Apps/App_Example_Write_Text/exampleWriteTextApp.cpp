#include <Arduino.h>
#include "mtb_text_scroll.h"
#include "mtb_engine.h"
#include "mtb_buzzer.h"
#include "exampleWriteTextApp.h"

ExampleApp_Data_t exampleAppInfo;

EXT_RAM_BSS_ATTR TaskHandle_t exampleApp_Task_H = NULL;

// button and encoder functions
void exampleAppButton(button_event_t button_Data);
void exampleAppEncoder(rotary_encoder_rotation_t direction);

// bluetooth functions
void exampleAppBleComTest(JsonDocument&);

EXT_RAM_BSS_ATTR Mtb_Applications_FullScreen *exampleWriteTextApp = new Mtb_Applications_FullScreen(exampleApp_Task, &exampleApp_Task_H, "exampleWriteTextApp", 4096);

void exampleApp_Task(void* dApplication){
// ****** Initialize the App Parameters
  Mtb_Applications *thisApp = (Mtb_Applications *)dApplication;
  thisApp->mtb_App_EncoderFn_ptr = exampleAppEncoder;
  thisApp->mtb_App_ButtonFn_ptr = exampleAppButton;
  //mtb_Ble_AppComm_Parser_Sv->mtb_Register_Ble_Comm_ServiceFns(exampleAppBleComTest);
  mtb_App_Init(thisApp);
// End of App parameter initialization

// ****** Recover Saved App settings from NVS
// mtb_Write_Nvs_Struct("exampleAppData", &exampleAppInfo, sizeof(exampleAppInfo));

// Declare Fixed and Scroll Text Variables
Mtb_FixedText_t exampleFixedText(24,15, Terminal8x12, GREEN);
Mtb_ScrollText_t exampleScrollText (5, 40, 118, WHITE, 20, 1, Terminal6x8);

// Write Fixed Text to display
exampleFixedText.mtb_Write_String("Hello World");

while (MTB_APP_IS_ACTIVE == pdTRUE) {
// Scroll the ScrollText Variable on display every 15 seconds
exampleScrollText.mtb_Scroll_This_Text("PIXLPAL - A project by Meterbit Cybernetics");
delay(15000);
}

// Clean up the application before exiting
  mtb_End_This_App(thisApp);
}

// ROTARY ENCODER CALLBACK FUNCTION
void exampleAppEncoder(rotary_encoder_rotation_t direction){
    if (direction == ROT_CLOCKWISE){
    do_beep(CLICK_BEEP);
    } else if(direction == ROT_COUNTERCLOCKWISE){
    do_beep(CLICK_BEEP);
    }
}


// BUTTON CALLBACK FUNCTION
void exampleAppButton(button_event_t button_Data){
            switch (button_Data.type){
            case BUTTON_RELEASED:
            do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED:
            do_beep(BEEP_0);
            break;

            case BUTTON_PRESSED_LONG:
            do_beep(BEEP_0);
            break;

            case BUTTON_CLICKED:
            //ESP_LOGI(TAG, "Button Clicked: %d Times\n",button_Data.count);
            switch (button_Data.count){
            case 1:
                break;
            case 2:
            do_beep(BEEP_0);
                break;
            case 3:
            do_beep(BEEP_0);
                break;
            default:
                break;
            }
                break;
            default:
            break;
			}
}


// BLE COMMUNICATION CALLBACK FUNCTION
// This function will be called when the application receives a command from the BLE client.
// void exampleAppBleComTest(JsonDocument& dCommand){
//     uint8_t cmdNumber = dCommand["app_command"];
//     String exampleCmd = dCommand["expl_Cmd"];

//     mtb_Write_Nvs_Struct("exampleAppData", &exampleAppInfo, sizeof(exampleAppInfo));
//     mtb_Ble_App_Cmd_Respond_Success(exampleAppRouteRoute, cmdNumber, pdPASS);
// }