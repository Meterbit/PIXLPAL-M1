#ifndef mtb_graphics
#define mtb_graphics

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
#include "mtb_fonts.h"
#include "mtb_colors.h"
#include "Arduino.h"
#include "hub75.h"
#include "mtb_nvs.h"

#define FIXED_TEXT_STYLE    1
#define SCROLL_TEXT_STYLE   0   

#define PANEL_RES_X 128    // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 64     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

#define ALM_BUZZER    48    // GPIO pin for the alarm buzzer
#define RGB_LED_PIN_R 3     // GPIO pin for the RGB status LED
#define RGB_LED_PIN_G 42    // GPIO pin for the RGB status LED
#define RGB_LED_PIN_B 38    // GPIO pin for the RGB status LED

    extern SemaphoreHandle_t onlinePNGsDrawReady_Sem;

    struct Mtb_LocalImage_t {           // Structure for local images stored in SPIFFS
        char imagePath[50] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int8_t scale = 1;
    };

    struct Mtb_OnlineImage_t {          // Structure for online images to be downloaded from URL
        char imageLink[300] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int8_t scale = 1;
    };

    typedef struct {
        Mtb_OnlineImage_t meta;         
        uint8_t* imageBuffer = nullptr;
        size_t imageSize = 0;
        bool isReady = false;
        bool failed = false;
    } Mtb_PreloadedImage_t;             // Structure for preloaded online images

    struct Mtb_LocalAnim_t {           // Structure for local Animations stored in SPIFFS
        char imagePath[50] = {0};
        uint16_t xAxis = 0;
        uint16_t yAxis = 0;
        int32_t loopCount = 1;
    };

    typedef struct{
        uint16_t color;		// 16-bit 565 color
        uint8_t brightness; // 0-255
    } rgb_led_message_t;

    extern void mtb_Do_Nothing_Void_Fn(void);
    typedef void (*ImgWipeFn_ptr)(void);

    extern Mtb_LocalImage_t statusBarItems[];
    extern uint16_t panelBrightness;
    extern uint16_t currentStatusLEDcolor;
    extern uint16_t mtb_Panel_Frame_Buffer[PANEL_RES_X][PANEL_RES_Y];

    // USER IMAGE DRAW FUNCTIONS
    extern BaseType_t mtb_Draw_Local_Png(const Mtb_LocalImage_t&);                                                                                          // Draw a local PNG image from SPIFFS
    extern bool mtb_Draw_Online_Png(const Mtb_OnlineImage_t* images, size_t drawPNGsCount = 1, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);    // Draw an online PNG image from URL
    extern void mtb_Download_Multi_Png(const Mtb_OnlineImage_t* images, size_t drawPNGsCount = 2);                                                              // Download multiple PNG images from URLs
    extern bool mtb_Draw_Multi_Png(size_t drawPNGsCount = 2, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);                                          // Draw multiple pre-downloaded PNG images

    extern BaseType_t mtb_Draw_Local_Svg(const Mtb_LocalImage_t&);                                                                                          // Draw a local SVG image from SPIFFS
    extern bool mtb_Draw_Online_Svg(const Mtb_OnlineImage_t* images, size_t drawSVGsCount = 1, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);    // Draw an online SVG image from URL
    extern void mtb_Download_Multi_Svg(const Mtb_OnlineImage_t* images, size_t drawSVGsCount = 2);                                                              // Download multiple SVG images from URLs
    extern bool mtb_Draw_Multi_Svg(size_t drawSVGsCount = 2, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);                                      // Draw multiple pre-downloaded SVG images
    
    extern uint8_t mtb_Draw_Local_Gif(const Mtb_LocalAnim_t &dAnim);                                                                                        // Draw a local GIF animation from SPIFFS     
                                                
    //************************************************************************************************************************************************* */

    extern void mtb_Set_Status_RGB_LED(uint16_t color, uint8_t brightness = (uint8_t) panelBrightness/2);       // Set the RGB LED color and brightness
    
    // SUPPORT FUNCTIONS
    extern void mtb_Show_Status_Bar_Icon(const Mtb_LocalImage_t&);
    extern void mtb_Wipe_Status_Bar_Icon(const Mtb_LocalImage_t &);
    extern void mtb_Draw_Status_Bar(void);
    extern String mtb_Get_Flag_Url_By_Country_Name(const String& countryName, const String& flagType);

    extern void mtb_Draw_Local_Png_Task(void *);
    extern void mtb_Draw_Local_Svg_Task(void *);

    extern QueueHandle_t nvsAccessQueue;
    extern QueueHandle_t rgb_led_queue;

    // GRAPHICS PROCESSING FUNCTIONS
    extern uint16_t mtb_Panel_Color565(uint8_t r, uint8_t g, uint8_t b, uint16_t *color = nullptr);
    extern uint16_t mtb_Panel_Color32bit_To_Color565(const char* dartColor, uint16_t *color565 = nullptr);
    extern void mtb_Panel_Draw_PixelRGB(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    extern void mtb_Panel_Draw_Pixel565(int16_t x, int16_t y, uint16_t color);
    extern void mtb_Panel_Draw_FrameRGB888(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *data);
    extern void mtb_Panel_Draw_FrameRGB565(int16_t x, int16_t y, int16_t width, int16_t height, const uint16_t *data);
    extern void mtb_Panel_Fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    extern void mtb_Panel_Set_Brightness(uint8_t brightness);
    extern void mtb_Panel_Fill_Screen(uint16_t color);
    extern void mtb_Panel_Clear_Screen(void);
    extern void mtb_Panel_Draw_HLine(int16_t y, int16_t x1, int16_t x2, uint16_t color);
    extern void mtb_Panel_Draw_VLine(int16_t x, int16_t y1, int16_t y2, uint16_t color);
    extern void mtb_Panel_Draw_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2,  uint16_t color);
    extern void mtb_Panel_Fill_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2,  uint16_t color);
    extern void mtb_Panel_Draw_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    
    extern void mtb_Panel_Draw_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    extern void mtb_Panel_Fill_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    extern void mtb_Panel_Draw_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    extern void mtb_Panel_Fill_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    extern void mtb_Panel_Draw_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color);
    extern void mtb_Panel_Fill_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color);
    // END OF SUPPORT FUNCTIONS *******************************************************************************************************


    // TEXT PROCESSING CLASSES
    class Mtb_Static_Text_t{
    public:
    int8_t yAxis, xAxis, charSpacing;
    uint8_t *fontMain;
    int16_t x1Seg, y1Seg, originX1Seg, originY1Seg;
    uint8_t textStyle;
    uint16_t textHorizSpace = 0;

    uint16_t color;
    uint16_t backgroundColor = BLACK;

    static void mtb_Config_Disp_Panel_Pins(void);
    static void mtb_Init_Led_Matrix_Panel();
    static void mtb_Clear_Screen(void);
    void setfont(const uint8_t *font);
    void writeXter(uint16_t a, int16_t x, int16_t y);

    // USER TEXT FUNCTIONS
    virtual uint16_t mtb_Write_String(const char *myString);    // Write a char array string to the display
    virtual uint16_t mtb_Write_String(String myString);         // Write a String object to the display

    virtual void mtb_Set_Pixel_Data(int16_t, int16_t){}
    virtual void mtb_Update_Panel_Segment(void){}
    virtual void mtb_Clear_Panel_Segment(void){}

    Mtb_Static_Text_t();
    Mtb_Static_Text_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8);

    virtual ~Mtb_Static_Text_t() {}  // Add this line
};


    // Inherit from Mtb_Static_Text_t to create a fixed text class with color and background color options
class Mtb_FixedText_t : public Mtb_Static_Text_t {
    public:
    static uint8_t** scratchPad;
    virtual void mtb_Set_Pixel_Data(int16_t, int16_t) override;
    virtual void mtb_Update_Panel_Segment(void) override;
    virtual void mtb_Clear_Panel_Segment(void) override;

    virtual uint16_t mtb_Write_String(const char *myString) override {return Mtb_Static_Text_t::mtb_Write_String(myString);}
    virtual uint16_t mtb_Write_String(String myString) override {return Mtb_Static_Text_t::mtb_Write_String(myString);}
        
    // FIXED TEXT USER FUNCTIONS ********************************************************************************
    virtual uint16_t mtb_Write_Colored_Text(const char *myString, uint16_t dColor);
    virtual uint16_t mtb_Write_Colored_Text(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);
    virtual uint16_t mtb_Write_Colored_Text(String myString, uint16_t dColor);
    virtual uint16_t mtb_Write_Colored_Text(String myString, uint16_t dColor, uint16_t dBackgroundColor);
    virtual uint16_t mtb_Clear_String();
    // END OF FIXED TEXT USER FUNCTIONS *********************************************************************************************************

    Mtb_FixedText_t();
    Mtb_FixedText_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) : Mtb_Static_Text_t(x1, y1, font){
        textStyle = FIXED_TEXT_STYLE;
        color = dColor;
        backgroundColor = dBackGrndColor;
    }

    // Overload the new operator
    void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

    // Overload the delete operator
    void operator delete(void* ptr) {heap_caps_free(ptr);}

    virtual ~Mtb_FixedText_t() {}  // Add this line
};


// Inherit from Mtb_FixedText_t to create a centered text class
class Mtb_CentreText_t : public Mtb_FixedText_t {
    public:

    uint16_t mtb_Write_String(const char *myString) override;    // Write a char array string to the display
    uint16_t mtb_Write_String(String myString) override;         // Write a String object to the display

    // FIXED TEXT USER FUNCTIONS ********************************************************************************
    uint16_t mtb_Write_Colored_Text(const char *myString, uint16_t dColor);
    uint16_t mtb_Write_Colored_Text(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);
    uint16_t mtb_Write_Colored_Text(String myString, uint16_t dColor);
    uint16_t mtb_Write_Colored_Text(String myString, uint16_t dColor, uint16_t dBackgroundColor);
    // END OF FIXED TEXT USER FUNCTIONS *********************************************************************************************************

    void mtb_Update_Panel_Segment(void) override {}
    void mtb_Clear_Panel_Segment(void) override;
    Mtb_CentreText_t();
    Mtb_CentreText_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) : Mtb_FixedText_t(x1, y1, font,dColor,dBackGrndColor){}

    // Overload the new operator
    void *operator new(size_t size){return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);}

    // Overload the delete operator
    void operator delete(void* ptr) {heap_caps_free(ptr);}
};

// SCROLL TEXT HELPER CLASS
class ScrollTextHelper_t : public Mtb_Static_Text_t {
    public:
    uint8_t ** scrollBuffer;
    virtual void mtb_Set_Pixel_Data(int16_t, int16_t) override;
    ScrollTextHelper_t();
    ScrollTextHelper_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8) : Mtb_Static_Text_t(x1, y1, font) { textStyle = SCROLL_TEXT_STYLE;}
};

extern Hub75Driver* mtb_Display_Driver;

#endif