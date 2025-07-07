
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Arduino.h"
#include "ledPanel.h"
#include "scrollMsgs.h"
#include "nvsMem.h"
#include "beep.h"
#include "mtbApps.h"


StaticQueue_t xQueueStorage_Scrolls[5];
EXT_RAM_BSS_ATTR TaskHandle_t scrollText_Handles[5] = {NULL};
EXT_RAM_BSS_ATTR QueueHandle_t scroll_Q[5] = {NULL};
EXT_RAM_BSS_ATTR Services* scroll_Tasks_Sv[5];
ScrollText_t* ScrollText_t::scrollTask_HolderPointers[5] = {nullptr};
//**************************************************************************************
ScrollText_t statusBarNotif (PINK_FLAMINGO);
//**************************************************************************************
ScrollText_t::ScrollText_t(uint16_t c, uint16_t p, uint8_t makeBeep){
    xPos = 0;
    yPos = 1;
    width = 128;
    color = c;
    speed = 30;
    pass = p;
    font = (uint8_t*)Terminal6x8;
    beep = makeBeep;
    height = font[6];
}

ScrollText_t::ScrollText_t(uint16_t xAxis, uint16_t yAxis, uint16_t width, uint16_t color){
    xPos = xAxis;
    yPos = yAxis;
    width = width;
    color = color;
    speed = 30;
    pass = 1;
    font = (uint8_t*)Terminal6x8;
    beep = 0;
    height = font[6];
}
//**************************************************************************************
ScrollText_t::ScrollText_t(uint16_t x, uint16_t y, uint16_t w, uint16_t c, uint16_t s, uint16_t p, const uint8_t * f, uint64_t scr_hld, uint8_t makeBeep){
    xPos = x;
    yPos = y;
    width = w;
    color = c;
    speed = s;
    pass = p;
    scroll_Hold = scr_hld;
    beep = makeBeep;
    font = (uint8_t*)f;
    height = font[6];
}
//**************************************************************************************
int countCharOccurrences(const char *str, char ch) {
    int count = 0;
    const char *ptr = str;

    while ((ptr = strchr(ptr, ch)) != NULL) {
        count++;
        ptr++;  // Move past the found character to search the rest of the string
    }

    return count;
}
//**************************************************************************************
void ScrollText_t::scrollString(){
    uint32_t scroll_Length = span + width;
    uint8_t yPos2 = font[6] + yPos;
    uint8_t xPos2 = width + xPos;
    uint64_t scroll_Hold_Counter = 0;

    UBaseType_t uxNumberOfMessages;

    if(beep) do_beep(beep,0);

    for (uint16_t b = 0; b < pass; b++){
        uxNumberOfMessages = uxQueueMessagesWaiting(scroll_Q[this->scrollTaskHandling]); //Know how many items are waiting to be processed.
        if(uxNumberOfMessages > 1 && pass > 1) break;
            for (uint32_t xPix = 0; xPix < scroll_Length; xPix++){
                if(scroll_Quit == pdTRUE) return; //if scroll_Quit is made True, scrolling stops and the function returns.
                for (uint16_t i = xPos, p = xPix; i < xPos2; i++, p++){
                    for (uint16_t j = yPos, q = 0; j < yPos2; j++, q++){
                        if (dText_Raw[p][q]) dma_display->drawPixel(i, j, color); // update color.
                        else dma_display->drawPixel(i, j, backgroundColor);         // update background color.
                    }
                }
                
                if(xPix == width){
                scroll_Hold_Counter = scroll_Hold;
                if(scroll_Hold == SCROLL_HOLD) break;
                else if(scroll_Hold > SCROLL_HOLD) while((scroll_Hold_Counter-->0) && (scroll_Quit == pdFALSE)) delay(1);
                } else delay(speed);
            }
    }
}
//**************************************************************************************
void scrollText_0_Task(void* dService){
    Services *thisService = (Services*) dService;
    if(Applications::currentRunningApp->showStatusBarClock == pdTRUE) statusBarClock_Sv->service_is_Running = pdFALSE;      // End the status bar clock service.

    ScrollText_t holder;
    ScrollText_t::scrollTask_HolderPointers[0] = &holder;

    while ((xQueuePeek(scroll_Q[0], &holder, pdMS_TO_TICKS(100)) == pdTRUE)){
        if(Applications::currentRunningApp->fullScreen == false) holder.scrollString();
            xQueueReceive(scroll_Q[0], &holder, pdMS_TO_TICKS(100));
            for(int i = 0; i < holder.stretch; i++) free(holder.dText_Raw[i]);
            free(holder.dText_Raw);
        }

    if(Applications::currentRunningApp->showStatusBarClock == pdTRUE) start_This_Service(statusBarClock_Sv);
    drawStatusBar();
    
    kill_This_Service(thisService);
}
//**************************************************************************************
void scrollText_1_Task(void* dService){
    Services *thisService = (Services*) dService;
    ScrollText_t holder;
    ScrollText_t::scrollTask_HolderPointers[1] = &holder;
    
    while(xQueuePeek(scroll_Q[1], &holder, pdMS_TO_TICKS(100)) == pdTRUE){
    holder.scrollString();
    xQueueReceive(scroll_Q[1], &holder, pdMS_TO_TICKS(100));
    for(int i = 0; i < holder.stretch; i++) free(holder.dText_Raw[i]);
    free(holder.dText_Raw);
    }
    kill_This_Service(thisService);
}
//**************************************************************************************
void scrollText_2_Task(void* dService){
    Services *thisService = (Services*) dService;
    ScrollText_t holder;
    ScrollText_t::scrollTask_HolderPointers[2] = &holder;

    while(xQueuePeek(scroll_Q[2], &holder, pdMS_TO_TICKS(100)) == pdTRUE){
        holder.scrollString();
        xQueueReceive(scroll_Q[2], &holder, pdMS_TO_TICKS(100));
    for(int i = 0; i < holder.stretch; i++) free(holder.dText_Raw[i]);
    free(holder.dText_Raw);
    }
    kill_This_Service(thisService);
}
//**************************************************************************************
void scrollText_3_Task(void* dService){
    Services *thisService = (Services*) dService;
    ScrollText_t holder;
    ScrollText_t::scrollTask_HolderPointers[3] = &holder;

    while(xQueuePeek(scroll_Q[3], &holder, pdMS_TO_TICKS(100)) == pdTRUE){
        holder.scrollString();
        xQueueReceive(scroll_Q[3], &holder, pdMS_TO_TICKS(100));
    for(int i = 0; i < holder.stretch; i++) free(holder.dText_Raw[i]);
    free(holder.dText_Raw);
    }

    kill_This_Service(thisService);
}
//**************************************************************************************
void scrollText_4_Task(void* dService){
    Services *thisService = (Services*) dService;
    ScrollText_t holder;
    ScrollText_t::scrollTask_HolderPointers[4] = &holder;

    while(xQueuePeek(scroll_Q[4], &holder, pdMS_TO_TICKS(100)) == pdTRUE){
        holder.scrollString();
        xQueueReceive(scroll_Q[4], &holder, pdMS_TO_TICKS(100));
    for(int i = 0; i < holder.stretch; i++) free(holder.dText_Raw[i]);
    free(holder.dText_Raw);
    }

    kill_This_Service(thisService);
}
//**************************************************************************************
void  text_Scrolls_Init(void){
    for (uint8_t i = 0; i < 5; i++){
    uint8_t *scrollQueues_buffer = (uint8_t *)heap_caps_malloc(40 * sizeof(ScrollText_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    scroll_Q[i] = xQueueCreateStatic(40, sizeof(ScrollText_t), scrollQueues_buffer, &xQueueStorage_Scrolls[i]); // These Queues live forever on device startup.
    scroll_Tasks_Sv[i] = (Services*) new Services();
    }
    for (uint8_t v = 0; v < 5; v++){
        scroll_Tasks_Sv[v]->serviceT_Handle_ptr = &scrollText_Handles[v];
        scroll_Tasks_Sv[v]->serviceCore = 0;
        scroll_Tasks_Sv[v]->servicePriority = 3;
        scroll_Tasks_Sv[v]->stackSize = 10240;
        snprintf(scroll_Tasks_Sv[v]->serviceName, 50, "%d", v);
        strcat(scroll_Tasks_Sv[v]->serviceName, " Scrl Txt Tsk");
        scroll_Tasks_Sv[v]->usePSRAM_Stack = pdTRUE;
    }
    scroll_Tasks_Sv[0]->service = scrollText_0_Task;
    scroll_Tasks_Sv[1]->service = scrollText_1_Task;
    scroll_Tasks_Sv[2]->service = scrollText_2_Task;
    scroll_Tasks_Sv[3]->service = scrollText_3_Task;
    scroll_Tasks_Sv[4]->service = scrollText_4_Task;
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(const char* dText){
    int spaceCount = countCharOccurrences(dText, ' ');
    ScrollText_t scroller;
    uint32_t charCount = strlen(dText);
    uint16_t fontHeight = font[6];
    uint8_t charSpacing = font[0];
    ScrollTextHelper_t scroll_ObjHolder(width, 0, font);
    span = 0;

    for (uint32_t i = 0; i < charCount; span += font[(dText[i] - 32) * 4 + 8] + charSpacing, ++i);
    span += spaceCount * font[1];
    stretch = span + (2 * width);

    // Create the 2D array
    dText_Raw = (uint8_t**)heap_caps_calloc(stretch, sizeof(uint8_t*), MALLOC_CAP_SPIRAM);
    for (int i = 0; i < stretch; i++) dText_Raw[i] = (uint8_t*)heap_caps_calloc(fontHeight, sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        
    scroll_ObjHolder.scrollBuffer = dText_Raw;
    scroll_ObjHolder.writeString(dText);

    for (int8_t i = 0, response = 0 ; i < 5; i++){         //This codeblock checks if the item to be scrolled matches the position of an item already scrolling. If yes, place
        response = xQueuePeek(scroll_Q[i], &scroller, pdMS_TO_TICKS(100));  //this new item in the queue same as the scrolling item so that it waits for the scrolling item to finish scrolling.
        if(response && (scroller.xPos == this->xPos) && (scroller.yPos == this->yPos)){
            this->scrollTaskHandling = i;
            xQueueSend(scroll_Q[i], this, portMAX_DELAY);
            return;
    }    
    }

    if(this == (&statusBarNotif)){
        xQueueSend(scroll_Q[0], this, portMAX_DELAY);
        start_This_Service(scroll_Tasks_Sv[0]);
    }else {
        for (uint8_t i = 1; i < 5; i++){
        if((scrollText_Handles[i]) == NULL){
            this->scrollTaskHandling = i;
            xQueueSend(scroll_Q[i], this, portMAX_DELAY);
            start_This_Service(scroll_Tasks_Sv[i]);
            break;
            }
        }
    }
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(const char* dText, uint16_t dcolor){
    color = dcolor;
    scroll_This_Text(dText);
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(const char* dText, uint16_t dcolor, uint16_t dPass){
    color = dcolor;
    pass = dPass;
    scroll_This_Text(dText);
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(const char* dText, uint16_t dcolor, uint16_t dPass, uint8_t dBeep){
    color = dcolor;
    pass = dPass;
    beep = dBeep;
    scroll_This_Text(dText);
}
//**************************************************************************************
//**************************************************************************************
void ScrollText_t::scroll_This_Text(String dText){
    scroll_This_Text(dText.c_str());
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(String dText, uint16_t dcolor){
    color = dcolor;
    scroll_This_Text(dText.c_str());
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(String dText, uint16_t dcolor, uint16_t dPass){
    color = dcolor;
    pass = dPass;
    scroll_This_Text(dText.c_str());
}
//**************************************************************************************
void ScrollText_t::scroll_This_Text(String dText, uint16_t dcolor, uint16_t dPass, uint8_t dBeep){
    color = dcolor;
    pass = dPass;
    beep = dBeep;
    scroll_This_Text(dText.c_str());
}
//**************************************************************************************
uint8_t ScrollText_t::scroll_Active(uint8_t check_or_stop){
    ScrollText_t holder;
        for (uint8_t i = 1; i < 5; i++){
        if((scrollText_Handles[i]) != NULL){
            xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(10));
            if(*this == holder){
                if(check_or_stop == STOP_SCROLL){
                while (xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(10)) == pdTRUE){
                   ScrollText_t::scrollTask_HolderPointers[i]->scroll_Quit = pdTRUE;
                   delay(1);
                } 
                dma_display->fillRect(holder.xPos, holder.yPos, holder.width, holder.height, holder.backgroundColor);
                }
                return 1;
                }
            }
        }
    return 0;
}

//**************************************************************************************
// uint8_t ScrollText_t::clear(){
//     ScrollText_t holder;
//         for (uint8_t i = 1; i < 5; i++){
//         if((scrollText_Handles[i]) != NULL){
//             xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(10));
//             if(*this == holder){
                
//                 while (xQueuePeek(scroll_Q[i], &holder, pdMS_TO_TICKS(10)) == pdTRUE){
//                    ScrollText_t::scrollTask_HolderPointers[i]->scroll_Quit = pdTRUE;
//                    delay(1);
//                 } 
//                 dma_display->fillRect(holder.xPos, holder.yPos, holder.width, holder.height, holder.backgroundColor);
                
//                 return 1;
//                 }
//             }
//         }
//     return 0;
// }