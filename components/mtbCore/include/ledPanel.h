#ifndef LEDPANEL
#define LEDPANEL

#include <stdio.h>
#include "driver/gpio.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "esp_system.h"
#include "esp_log.h"
#include "fonts.h"
#include "mtbColors.h"
#include "Arduino.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "nvsMem.h"

#define STATIC_STYLE    1
#define DYNAMIC_STYLE   0

#define MATRIX_WIDTH 128
#define MATRIX_HEIGHT 64

#define PANEL_RES_X 128      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 64     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

#define ALM_BUZZER    48
#define RGB_LED_PIN_R 3
#define RGB_LED_PIN_G 42
#define RGB_LED_PIN_B 38

    extern SemaphoreHandle_t onlinePNGsDrawReady_Sem;

    struct PNG_LocalImage_t {
        char imagePath[50] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int8_t scale = 1;
    };

    struct PNG_OnlineImage_t {
        char imageLink[300] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int8_t scale = 1;
    };


    struct SVG_OnlineImage_t {
        char imageLink[300] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int8_t scale = 1;
    };

    typedef struct {
        PNG_OnlineImage_t meta;       // original input
        uint8_t* pngBuffer = nullptr;
        size_t pngSize = 0;
        bool isReady = false;
        bool failed = false;
    } PNG_PreloadedImage_t;


typedef struct {
    SVG_OnlineImage_t meta;
    uint8_t* svgBuffer = nullptr;
    size_t svgSize = 0;
    bool isReady = false;
    bool failed = false;
} SVG_PreloadedImage_t;



typedef struct
{
	uint16_t color;		// 16-bit 565 color
	uint8_t brightness; // 0-255
} rgb_led_message_t;

    extern void doNothingVoidFn(void);
typedef void (*ImgWipeFn_ptr)(void);

    extern PNG_LocalImage_t statusBarItems[];
    extern uint16_t panelBrightness;
    extern uint16_t currentStatusLEDcolor;

    extern void mtb_Time_Setup_Init(void);

    extern BaseType_t drawLocalPNG(const PNG_LocalImage_t&);
    extern void drawLocalPNG_Task(void *);   


    extern BaseType_t drawOnlinePNG(const PNG_OnlineImage_t&);
    extern void drawOnlinePNG_Task(void *);

    extern void downloadMultipleOnlinePNGs(const PNG_OnlineImage_t* images, size_t count);
    extern bool drawMultiplePNGs(size_t drawPNGsCount, ImgWipeFn_ptr wipePreviousImgs = doNothingVoidFn);

    extern BaseType_t drawOnlineSVG(const SVG_OnlineImage_t&);
    extern void drawOnlineSVG_Task(void *);

    extern void downloadMultipleOnlineSVGs(const SVG_OnlineImage_t* images, size_t count);
    extern bool drawMultipleSVGs(size_t drawSVGsCount, ImgWipeFn_ptr wipePreviousImgs = doNothingVoidFn);

    extern void showStatusBarIcon(const PNG_LocalImage_t&);
    extern void wipeStatusBarIcon(const PNG_LocalImage_t &);

    extern void set_Status_RGB_LED(uint16_t color, uint8_t brightness = (uint8_t) panelBrightness/2);
    extern void drawStatusBar(void);

    extern QueueHandle_t nvsAccessQueue;
    extern QueueHandle_t rgb_led_queue;

    class Matrix_Panel_t{
    public:
    uint8_t yAxis, xAxis, charSpacing;
    uint8_t *fontMain;
    uint16_t x1Seg, y1Seg, originX1Seg, originY1Seg;
    uint8_t textStyle;
    uint16_t textHorizSpace = 0;

    static void config_ESP32_Panel_Pins(void);
    static void init_LED_MatrixPanel();
    static void clearScreen(void);

    void setfont(const uint8_t *font);
    void writeXter(uint16_t a, uint16_t x, uint16_t y);
    uint16_t writeString(const char *myString);
    uint16_t writeString(String myString);

    virtual void set_Pixel_Data(uint16_t, uint16_t){}
    virtual void updatePanelSegment(void){}
    virtual void clearPanelSegment(void){}

    Matrix_Panel_t();
    Matrix_Panel_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8);

    virtual ~Matrix_Panel_t() {}  // Add this line
};

class FixedText_t : public Matrix_Panel_t {
    public:
    static uint8_t** scratchPad;
    uint16_t color;
    uint16_t backgroundColor = BLACK;
    virtual void set_Pixel_Data(uint16_t, uint16_t);
    virtual uint16_t writeColoredString(const char *myString, uint16_t dColor);
    virtual uint16_t writeColoredString(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);
    virtual uint16_t writeColoredString(String myString, uint16_t dColor);
    virtual uint16_t writeColoredString(String myString, uint16_t dColor, uint16_t dBackgroundColor);
    virtual uint16_t clearString();
    virtual void updatePanelSegment(void);
    virtual void clearPanelSegment(void);
    FixedText_t();
    FixedText_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) : Matrix_Panel_t(x1, y1, font){
        textStyle = STATIC_STYLE;
        color = dColor;
        backgroundColor = dBackGrndColor;
    }

    // Overload the new operator
    void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

    // Overload the delete operator
    void operator delete(void* ptr) {heap_caps_free(ptr);}

    virtual ~FixedText_t() {}  // Add this line
};

class CentreText_t : public FixedText_t {
    public:
    uint16_t writeColoredString(const char *myString, uint16_t dColor);
    uint16_t writeColoredString(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);
    uint16_t writeColoredString(String myString, uint16_t dColor);
    uint16_t writeColoredString(String myString, uint16_t dColor, uint16_t dBackgroundColor);
    void updatePanelSegment(void){}
    void clearPanelSegment(void);
    CentreText_t();
    CentreText_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) : FixedText_t(x1, y1, font,dColor,dBackGrndColor){}

    // Overload the new operator
    void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

    // Overload the delete operator
    void operator delete(void* ptr) {heap_caps_free(ptr);}
};

class ScrollTextHelper_t : public Matrix_Panel_t {
    public:
    uint8_t ** scrollBuffer;
    virtual void set_Pixel_Data(uint16_t, uint16_t);
    ScrollTextHelper_t();
    ScrollTextHelper_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8) : Matrix_Panel_t(x1, y1, font) {
        textStyle = DYNAMIC_STYLE;
        }
};

    extern MatrixPanel_I2S_DMA *dma_display;
#endif