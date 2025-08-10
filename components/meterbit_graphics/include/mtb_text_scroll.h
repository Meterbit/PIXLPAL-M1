#ifndef _SCROLL_MSG
#define _SCROLL_MSG

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "mtb_fonts.h"
#include "mtb_colors.h"
#include "Arduino.h"

#define CHECK_SCROLL        0
#define STOP_SCROLL         1
#define CHAR_LEN            1005
// #define SCROLL_PIX_WIDTH    6000
// #define SCROLL_PIX_HEIGHT   9
#define DEF_SPEED 25

#define SCROLL_HOLD 1

extern TaskHandle_t scrollText_Handles[];
extern QueueHandle_t scroll_Q[];
extern TaskHandle_t scrollQ_Handle;
extern uint8_t taskState;
extern TaskHandle_t message_Scroll;

extern uint8_t stopCurrentScroll;
extern uint16_t scrollLength;								//Depicts the span of X axis of the scrolling Text.
extern uint16_t scrollSpeed;
extern int16_t scrollCount;

class ScrollText_t {
        public: 
        uint16_t xPos;
        uint16_t yPos;
        uint16_t width;
        uint16_t height;
        uint16_t color;
        uint16_t backgroundColor = BLACK;
        uint16_t speed;
        uint16_t pass;
        uint8_t *font;
        uint8_t** dText_Raw;
        uint64_t span;
        uint64_t stretch;
        uint64_t scroll_Hold = pdFALSE;
        uint8_t scroll_Quit = pdFALSE;
        uint8_t beep;

        uint8_t scrollTaskHandling = 0;      // This is used to set the scroll task that is running the scrolling for this particular object.
        static ScrollText_t *scrollTask_HolderPointers[];

        void scrollString();
        void scroll_This_Text(const char*);
        void scroll_This_Text(const char*, uint16_t c);
        void scroll_This_Text(const char*, uint16_t c, uint16_t p);
        void scroll_This_Text(const char*, uint16_t c, uint16_t p, uint8_t b);
        void scroll_This_Text(String dText);
        void scroll_This_Text(String, uint16_t c);
        void scroll_This_Text(String, uint16_t c, uint16_t p);
        void scroll_This_Text(String, uint16_t c, uint16_t p, uint8_t b);
        uint8_t scroll_Active(uint8_t check_or_stop = CHECK_SCROLL);    // Checks if the current scrollTextObj is active or not, and decide whether to stop it or not.
        //uint8_t clear();    // Checks if the current scrollTextObj is active or not, and decide whether to stop it or not.
        
        // Overload the new operator
        void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

        // Overload the delete operator
        void operator delete(void* ptr) {heap_caps_free(ptr);}

        // Overload the == operator
        bool operator==(const ScrollText_t &other) const {
            return (xPos == other.xPos) && (yPos == other.yPos);
        }

        ScrollText_t(){}
        // ScrollText_t(const uint8_t *f) { font = (uint8_t*) f; }
        // ScrollText_t(uint16_t x, uint16_t y, uint16_t w, uint16_t c, const uint8_t* f);
        ScrollText_t(uint16_t xAxis, uint16_t yAxis, uint16_t width, uint16_t color, uint16_t speed, uint16_t pass, const uint8_t* font, uint64_t scr_hld = 0, uint8_t makeBeep = 0);
        ScrollText_t(uint16_t xAxis, uint16_t yAxis, uint16_t width, uint16_t color);
        ScrollText_t(uint16_t color, uint16_t pass = 1, uint8_t makeBeep = 0);
};

extern void text_Scrolls_Init(void);
void scrollText_0_Task(void *arguments);
extern ScrollText_t statusBarNotif;


/*
typedef struct {
    uint16_t color[3];
    uint16_t speed;
    uint8_t count;
    char dInfo[100];
}scroll_Msg_Var_t;
*/

// extern const ScrollText_t calendar_Set;
// extern const ScrollText_t tempor_Shutdown;
// extern const ScrollText_t device_Restart;
// extern const ScrollText_t newOTA_Update;
// extern const ScrollText_t ntp_Time_Set;
// extern const ScrollText_t shutDown_Less_2Mins;
// extern const ScrollText_t file_not_found;

#endif









