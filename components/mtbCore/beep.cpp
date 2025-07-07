#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include <unistd.h>
#include "driver/timer.h"

#include "ledPanel.h"
#include "beep.h"
#include "Arduino.h"
#include "mtbApps.h"

EXT_RAM_BSS_ATTR SemaphoreHandle_t beep_Duration_Sem = NULL; 
EXT_RAM_BSS_ATTR TimerHandle_t beepTimerHandle10 = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t beepTimerHandle200 = NULL;
EXT_RAM_BSS_ATTR TimerHandle_t beepTimerHandle1000 = NULL;
EXT_RAM_BSS_ATTR TaskHandle_t beepTaskHandle = NULL;
uint8_t beep_TYPE = 0;
uint8_t beep_COUNT = 0;

EXT_RAM_BSS_ATTR Services *beep_Buzzer_Sv = new Services(buzzer_Beep, &beepTaskHandle, "Beep Sound", 2048, 3, pdTRUE);

void do_beep(uint8_t beep_Typ, uint8_t beep_Cnt){
    beep_COUNT = beep_Cnt;
    beep_TYPE = beep_Typ;
    start_This_Service(beep_Buzzer_Sv);
}

void beepStop(TimerHandle_t timer200){
    xSemaphoreGive(beep_Duration_Sem);
}

void make_Beep(TimerHandle_t dTimer_H){

    digitalWrite(ALM_BUZZER, HIGH);
    xTimerStart(dTimer_H, 0);
    xSemaphoreTake(beep_Duration_Sem, portMAX_DELAY); 
    digitalWrite(ALM_BUZZER, LOW);                                                                  // HARDWARE GPIO DOES NOT FOLLOW SOFTWARE COMMAND SEQUENCE. FIND BUG.
    //("Shoot Alarm\n");
    delay(200);
}

void buzzer_Beep(void* dService){         // Beep sound task
Services *thisServ = (Services *)dService;
    uint8_t beep_TRACK;
    if(beep_Duration_Sem == NULL) beep_Duration_Sem = xSemaphoreCreateBinary();
    if(beepTimerHandle200 == NULL) beepTimerHandle200 = xTimerCreate("beepTimer200", pdMS_TO_TICKS(200), pdFALSE, NULL, beepStop);
    if(beepTimerHandle1000 == NULL) beepTimerHandle1000 = xTimerCreate("beepTimer1000", pdMS_TO_TICKS(1000), pdFALSE, NULL, beepStop);
    if(beepTimerHandle10 == NULL) beepTimerHandle10 = xTimerCreate("beepTimer200", pdMS_TO_TICKS(10), pdFALSE, NULL, beepStop);
        
    while(beep_COUNT-->0 && THIS_SERV_IS_ACTIVE){
        beep_TRACK = beep_TYPE;
        switch (beep_TYPE){
        case BEEP_0: 
            make_Beep(beepTimerHandle1000);
            //make_Beep(beepTimerHandle1000);
            break;

        case BEEP_1:
            while(beep_TRACK-->0) make_Beep(beepTimerHandle200);
            break;

        case BEEP_2:
            while(beep_TRACK-->0) make_Beep(beepTimerHandle200);
            break;
        
        case BEEP_3:
            while(beep_TRACK-->0) make_Beep(beepTimerHandle200);
            break;

        case BEEP_4:
            while(beep_TRACK-->0) make_Beep(beepTimerHandle200);
            break;

        case CLICK_BEEP:
            make_Beep(beepTimerHandle10);
            break;
        default:
            break;
        }
        delay(1000);
        //printf("ONE SEC DELAY\n");
    }
// CREATED TIMERS IN THIS TASK SHOULD BE DELETED HERE IF NEEDED.
    kill_This_Service(thisServ);
}
