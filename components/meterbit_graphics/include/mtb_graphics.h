/**
 * @file mtb_graphics.h
 * @brief PIXLPAL-M1 graphics rendering API for the 128×64 HUB75 RGB LED matrix.
 *
 * Provides:
 *   - **Image rendering**: PNG and SVG images from LittleFS (`mtb_Draw_Local_Png`,
 *     `mtb_Draw_Local_Svg`) or live URLs (`mtb_Draw_Online_Png`, `mtb_Draw_Online_Svg`),
 *     with multi-image pre-download helpers.
 *   - **GIF animation**: `mtb_Draw_Local_Gif` for frame-by-frame LittleFS GIFs.
 *   - **Primitives**: pixel, line, H/V line, rectangle, circle, triangle, rounded-rectangle
 *     draw and fill operations in RGB or RGB565 format.
 *   - **Frame buffer helpers**: `mtb_Panel_Fill`, `mtb_Panel_Fill_Screen`,
 *     `mtb_Panel_Clear_Screen`, full-screen RGB565/RGB888 frame blits.
 *   - **Text widgets**: `Mtb_Static_Text_t` (base), `Mtb_FixedText_t` (fixed position),
 *     `Mtb_CentreText_t` (auto-centred), `ScrollTextHelper_t` (internal, used by the
 *     scroll subsystem).
 *   - **Status bar**: `mtb_Show_Status_Bar_Icon`, `mtb_Wipe_Status_Bar_Icon`,
 *     `mtb_Draw_Status_Bar` for the 8-pixel-tall icon strip.
 *   - **RGB LED**: `mtb_Set_Status_RGB_LED` for the onboard RGB status indicator.
 *   - **Color conversions**: `mtb_Panel_Color565`, `mtb_Panel_Color32bit_To_Color565`.
 *
 * All PSRAM-allocated widget objects override new/delete to use MALLOC_CAP_SPIRAM.
 * The lodepng decoder (lodepng.h / lodepng_psram.cpp) is a third-party dependency
 * and is intentionally not documented here.
 */
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

/** @defgroup mtb_text_styles Text Widget Style Constants
 * @{ */
#define FIXED_TEXT_STYLE  1 /**< Widget renders text at a fixed X/Y position. */
#define SCROLL_TEXT_STYLE 0 /**< Widget renders text into a scroll pixel buffer. */
/** @} */

/** @defgroup mtb_panel_dimensions Panel Resolution Constants
 * @{ */
#define PANEL_RES_X 128 /**< Width in pixels of the HUB75 panel (one module). */
#define PANEL_RES_Y 64  /**< Height in pixels of the HUB75 panel (one module). */
#define PANEL_CHAIN 1   /**< Number of HUB75 modules chained together. */
/** @} */

/** @defgroup mtb_gpio_pins Hardware GPIO Assignments
 * @{ */
#define ALM_BUZZER    48 /**< GPIO driving the piezo alarm buzzer. */
#define RGB_LED_PIN_R  3 /**< GPIO driving the red channel of the onboard RGB status LED. */
#define RGB_LED_PIN_G 42 /**< GPIO driving the green channel of the onboard RGB status LED. */
#define RGB_LED_PIN_B 38 /**< GPIO driving the blue channel of the onboard RGB status LED. */
/** @} */

extern SemaphoreHandle_t onlinePNGsDrawReady_Sem; /**< Semaphore given when all pre-downloaded PNG images are ready to render. */

/**
 * @brief Descriptor for an image stored on LittleFS.
 */
struct Mtb_LocalImage_t {
    char     imagePath[50] = {0}; /**< LittleFS path to the PNG or SVG file (e.g. "/icons/logo.png"). */
    uint16_t xAxis = 0;           /**< X-axis pixel position for the top-left corner of the image. */
    uint16_t yAxis = 0;           /**< Y-axis pixel position for the top-left corner of the image. */
    int8_t   scale = 1;           /**< Scale factor; 1 = original size (only 1 is currently supported). */
};

/**
 * @brief Descriptor for an image to be fetched from a URL at runtime.
 */
struct Mtb_OnlineImage_t {
    const char* imageLink = nullptr; /**< HTTP/HTTPS URL string; must remain valid until the download completes (string literals are ideal). */
    uint16_t    xAxis = 0;           /**< X-axis pixel position for the top-left corner. */
    uint16_t    yAxis = 0;           /**< Y-axis pixel position for the top-left corner. */
    int8_t      scale = 1;           /**< Scale factor; 1 = original size. */
};

/**
 * @brief Pre-downloaded image buffer used by the multi-PNG batch-render flow.
 */
typedef struct {
    Mtb_OnlineImage_t meta;       /**< Original image URL and position descriptor. */
    uint8_t*          imageBuffer = nullptr; /**< PSRAM buffer holding the raw PNG bytes; free after rendering. */
    size_t            imageSize   = 0;       /**< Size of @p imageBuffer in bytes. */
    bool              isReady     = false;   /**< true when the download completed successfully. */
    bool              failed      = false;   /**< true if the download failed. */
} Mtb_PreloadedImage_t;

/**
 * @brief Descriptor for a GIF animation stored on LittleFS.
 */
struct Mtb_LocalAnim_t {
    char     imagePath[50] = {0}; /**< LittleFS path to the GIF file. */
    uint16_t xAxis = 0;           /**< X-axis position for the top-left corner. */
    uint16_t yAxis = 0;           /**< Y-axis position for the top-left corner. */
    int32_t  loopCount = 1;       /**< Number of times to loop the animation (-1 = infinite, 0 = forever). */
};

/**
 * @brief Message queued to the RGB LED control task.
 */
typedef struct {
    uint16_t color;      /**< Desired LED colour (RGB565). */
    uint8_t  brightness; /**< LED brightness (0–255). */
} rgb_led_message_t;

extern void mtb_Do_Nothing_Void_Fn(void);         /**< No-op function used as a default wipe callback. */
typedef void (*ImgWipeFn_ptr)(void);               /**< Callback type invoked to clear the screen before drawing a new image. */

extern Mtb_LocalImage_t statusBarItems[];          /**< Array of icon descriptors currently shown in the status bar. */
extern uint16_t panelBrightness;                   /**< Current HUB75 panel brightness level (0–255). */
extern uint16_t currentStatusLEDcolor;             /**< Current RGB status LED colour (RGB565). */
extern uint16_t mtb_Panel_Frame_Buffer[PANEL_RES_X][PANEL_RES_Y]; /**< Software frame buffer mirroring the panel state. */

/* ---- Image draw functions ---- */

/**
 * @brief Decode and draw a PNG from LittleFS to the panel.
 * @param image  Descriptor specifying the path and position.
 * @return pdPASS if the image was drawn successfully, pdFAIL otherwise.
 */
extern BaseType_t mtb_Draw_Local_Png(const Mtb_LocalImage_t& image);

/**
 * @brief Download and draw one or more PNG images from URLs.
 * @param images         Array of Mtb_OnlineImage_t descriptors.
 * @param drawPNGsCount  Number of images in the array (default 1).
 * @param wipePreviousImgs  Called before drawing to clear the previous content (default no-op).
 * @return true on success, false if any download or decode fails.
 */
extern bool mtb_Draw_Online_Png(const Mtb_OnlineImage_t* images, size_t drawPNGsCount = 1, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);

/**
 * @brief Pre-download multiple PNG images into PSRAM buffers without drawing them yet.
 * @param images         Array of Mtb_OnlineImage_t descriptors.
 * @param drawPNGsCount  Number of images to download (default 2).
 */
extern void mtb_Download_Multi_Png(const Mtb_OnlineImage_t* images, size_t drawPNGsCount = 2);

/**
 * @brief Draw multiple previously downloaded PNG images from their PSRAM buffers.
 * @param drawPNGsCount     Number of images to draw (default 2).
 * @param wipePreviousImgs  Called before drawing to clear the previous content (default no-op).
 * @return true if all images were drawn successfully.
 */
extern bool mtb_Draw_Multi_Png(size_t drawPNGsCount = 2, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);

/**
 * @brief Decode and draw an SVG from LittleFS to the panel.
 * @param image  Descriptor specifying the path and position.
 * @return pdPASS if drawn, pdFAIL otherwise.
 */
extern BaseType_t mtb_Draw_Local_Svg(const Mtb_LocalImage_t& image);

/**
 * @brief Download and draw one or more SVG images from URLs.
 * @param images         Array of Mtb_OnlineImage_t descriptors.
 * @param drawSVGsCount  Number of images (default 1).
 * @param wipePreviousImgs  Wipe callback (default no-op).
 * @return true on success.
 */
extern bool mtb_Draw_Online_Svg(const Mtb_OnlineImage_t* images, size_t drawSVGsCount = 1, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);

/**
 * @brief Pre-download multiple SVG images into PSRAM without drawing them.
 * @param images         Array of Mtb_OnlineImage_t descriptors.
 * @param drawSVGsCount  Number of images (default 2).
 */
extern void mtb_Download_Multi_Svg(const Mtb_OnlineImage_t* images, size_t drawSVGsCount = 2);

/**
 * @brief Draw multiple previously downloaded SVG images from their PSRAM buffers.
 * @param drawSVGsCount    Number of images to draw (default 2).
 * @param wipePreviousImgs Wipe callback (default no-op).
 * @return true if all images were drawn successfully.
 */
extern bool mtb_Draw_Multi_Svg(size_t drawSVGsCount = 2, ImgWipeFn_ptr wipePreviousImgs = mtb_Do_Nothing_Void_Fn);

/**
 * @brief Play a GIF animation stored on LittleFS.
 * @param dAnim  Descriptor specifying the path, position, and loop count.
 * @return 0 on normal completion, non-zero if the animation was aborted early.
 */
extern uint8_t mtb_Draw_Local_Gif(const Mtb_LocalAnim_t &dAnim);

/**
 * @brief Set the onboard RGB status LED to a given colour and brightness.
 * @param color       Desired colour (RGB565).
 * @param brightness  LED brightness (0–255, default panelBrightness/2).
 */
extern void mtb_Set_Status_RGB_LED(uint16_t color, uint8_t brightness = (uint8_t) panelBrightness/2);

/* ---- Status-bar icon functions ---- */

/**
 * @brief Draw a status-bar icon image at its recorded position.
 * @param icon  Descriptor of the icon to show (path, x, y).
 */
extern void mtb_Show_Status_Bar_Icon(const Mtb_LocalImage_t& icon);

/**
 * @brief Erase a status-bar icon by filling its region with the background colour.
 * @param icon  Descriptor of the icon to wipe (path, x, y).
 */
extern void mtb_Wipe_Status_Bar_Icon(const Mtb_LocalImage_t& icon);

/**
 * @brief Redraw all registered status-bar icons (called after a full-screen app exits).
 */
extern void mtb_Draw_Status_Bar(void);

/**
 * @brief Build a country-flag URL string from a country name and flag aspect ratio.
 * @param countryName  Country name string (e.g. "Japan").
 * @param flagType     Aspect ratio tag (e.g. "4x3" or "1x1").
 * @return Fully-formed URL string for the flag SVG asset.
 */
extern String mtb_Get_Flag_Url_By_Country_Name(const String& countryName, const String& flagType);

/**
 * @brief FreeRTOS service task entry point for drawing PNG images from LittleFS.
 * @param pvParam  Unused parameter.
 */
extern void mtb_Draw_Local_Png_Task(void *pvParam);

/**
 * @brief FreeRTOS service task entry point for drawing SVG images from LittleFS.
 * @param pvParam  Unused parameter.
 */
extern void mtb_Draw_Local_Svg_Task(void *pvParam);

extern QueueHandle_t nvsAccessQueue; /**< Forward-declared NVS queue (primary definition in mtb_engine.h). */
extern QueueHandle_t rgb_led_queue;  /**< Queue for rgb_led_message_t messages sent to the LED control task. */

/* ---- Graphics processing functions ---- */

/**
 * @brief Pack R/G/B byte values into a 16-bit RGB565 word.
 * @param r      Red component (0–255).
 * @param g      Green component (0–255).
 * @param b      Blue component (0–255).
 * @param color  Optional output pointer; also writes the result here if not nullptr.
 * @return Packed RGB565 value.
 */
extern uint16_t mtb_Panel_Color565(uint8_t r, uint8_t g, uint8_t b, uint16_t *color = nullptr);

/**
 * @brief Convert a 32-bit HTML/Dart color string ("#RRGGBB" or "0xAARRGGBB") to RGB565.
 * @param dartColor  Color string.
 * @param color565   Optional output pointer; also writes the result here if not nullptr.
 * @return RGB565 value.
 */
extern uint16_t mtb_Panel_Color32bit_To_Color565(const char* dartColor, uint16_t *color565 = nullptr);

/**
 * @brief Blit a packed 24-bit RGB888 frame buffer sub-region to the panel.
 * @param x      Top-left X coordinate.
 * @param y      Top-left Y coordinate.
 * @param width  Width of the region in pixels.
 * @param height Height of the region in pixels.
 * @param data   Pointer to the source RGB888 byte array (3 bytes per pixel, row-major).
 */
extern void mtb_Panel_Draw_FrameRGB888(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *data);

/**
 * @brief Blit a packed 16-bit RGB565 frame buffer sub-region to the panel.
 * @param x      Top-left X coordinate.
 * @param y      Top-left Y coordinate.
 * @param width  Width of the region in pixels.
 * @param height Height of the region in pixels.
 * @param data   Pointer to the source RGB565 word array (1 word per pixel, row-major).
 */
extern void mtb_Panel_Draw_FrameRGB565(int16_t x, int16_t y, int16_t width, int16_t height, const uint16_t *data);

/**
 * @brief Copy a full 128×64 RGB565 frame buffer to the panel.
 * @param data  Source frame buffer array [128][64].
 */
extern void mtb_Panel_Draw_FullScreen_RGB565(uint16_t data[128][64]);

/**
 * @brief Fill a rectangular region of the panel with a solid RGB colour.
 * @param x  Top-left X.
 * @param y  Top-left Y.
 * @param w  Width in pixels.
 * @param h  Height in pixels.
 * @param r  Red component (0–255).
 * @param g  Green component (0–255).
 * @param b  Blue component (0–255).
 */
extern void mtb_Panel_Fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set the HUB75 panel PWM brightness.
 * @param brightness  Brightness level (0–255; 0 = off, 255 = maximum).
 */
extern void mtb_Panel_Set_Brightness(uint8_t brightness);

/**
 * @brief Fill the entire panel with a single RGB565 colour.
 * @param color  Fill colour (RGB565).
 */
extern void mtb_Panel_Fill_Screen(uint16_t color);

/**
 * @brief Clear the entire panel to black.
 */
extern void mtb_Panel_Clear_Screen(void);

/* ---- Pixel and shape drawing primitives ---- */

/**
 * @brief Draw a single pixel at (x, y) with the given RGB colour.
 * @param x  X coordinate (0–127).
 * @param y  Y coordinate (0–63).
 * @param r  Red component.
 * @param g  Green component.
 * @param b  Blue component.
 */
extern void mtb_Panel_Draw_PixelRGB(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Draw a single pixel at (x, y) with the given RGB565 colour.
 * @param x      X coordinate.
 * @param y      Y coordinate.
 * @param color  Pixel colour (RGB565).
 */
extern void mtb_Panel_Draw_Pixel565(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Draw a straight line from (x1, y1) to (x2, y2).
 * @param x1    Start X.
 * @param y1    Start Y.
 * @param x2    End X.
 * @param y2    End Y.
 * @param color Line colour (RGB565).
 */
extern void mtb_Panel_Draw_Line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief Draw a horizontal line at row @p y from column @p x1 to @p x2.
 * @param y     Row Y coordinate.
 * @param x1    Start X.
 * @param x2    End X.
 * @param color Line colour (RGB565).
 */
extern void mtb_Panel_Draw_HLine(int16_t y, int16_t x1, int16_t x2, uint16_t color);

/**
 * @brief Draw a vertical line at column @p x from row @p y1 to @p y2.
 * @param x     Column X coordinate.
 * @param y1    Start Y.
 * @param y2    End Y.
 * @param color Line colour (RGB565).
 */
extern void mtb_Panel_Draw_VLine(int16_t x, int16_t y1, int16_t y2, uint16_t color);

/**
 * @brief Draw an unfilled rectangle outline.
 * @param x1    Top-left X.
 * @param y1    Top-left Y.
 * @param x2    Bottom-right X.
 * @param y2    Bottom-right Y.
 * @param color Outline colour (RGB565).
 */
extern void mtb_Panel_Draw_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief Draw a filled rectangle.
 * @param x1    Top-left X.
 * @param y1    Top-left Y.
 * @param x2    Bottom-right X.
 * @param y2    Bottom-right Y.
 * @param color Fill colour (RGB565).
 */
extern void mtb_Panel_Fill_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief Draw an unfilled circle outline.
 * @param x0    Centre X.
 * @param y0    Centre Y.
 * @param r     Radius in pixels.
 * @param color Outline colour (RGB565).
 */
extern void mtb_Panel_Draw_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

/**
 * @brief Draw a filled circle.
 * @param x0    Centre X.
 * @param y0    Centre Y.
 * @param r     Radius in pixels.
 * @param color Fill colour (RGB565).
 */
extern void mtb_Panel_Fill_Circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

/**
 * @brief Draw an unfilled triangle outline through three vertices.
 * @param x0    Vertex 0 X.
 * @param y0    Vertex 0 Y.
 * @param x1    Vertex 1 X.
 * @param y1    Vertex 1 Y.
 * @param x2    Vertex 2 X.
 * @param y2    Vertex 2 Y.
 * @param color Outline colour (RGB565).
 */
extern void mtb_Panel_Draw_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief Draw a filled triangle.
 * @param x0    Vertex 0 X.
 * @param y0    Vertex 0 Y.
 * @param x1    Vertex 1 X.
 * @param y1    Vertex 1 Y.
 * @param x2    Vertex 2 X.
 * @param y2    Vertex 2 Y.
 * @param color Fill colour (RGB565).
 */
extern void mtb_Panel_Fill_Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief Draw an unfilled rounded-corner rectangle.
 * @param x1     Top-left X.
 * @param y1     Top-left Y.
 * @param x2     Bottom-right X.
 * @param y2     Bottom-right Y.
 * @param radius Corner radius in pixels.
 * @param color  Outline colour (RGB565).
 */
extern void mtb_Panel_Draw_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color);

/**
 * @brief Draw a filled rounded-corner rectangle.
 * @param x1     Top-left X.
 * @param y1     Top-left Y.
 * @param x2     Bottom-right X.
 * @param y2     Bottom-right Y.
 * @param radius Corner radius in pixels.
 * @param color  Fill colour (RGB565).
 */
extern void mtb_Panel_Fill_Round_Rect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t radius, uint16_t color);

/* ---- Text rendering classes ---- */

/**
 * @brief Base text renderer for the MikroElektronika GLCD font format.
 *
 * Renders a null-terminated string or Arduino String character by character
 * using the configured GLCD font. Subclasses override mtb_Set_Pixel_Data(),
 * mtb_Update_Panel_Segment(), and mtb_Clear_Panel_Segment() to control how and
 * where pixels are written.
 */
class Mtb_Static_Text_t {
public:
    int8_t   yAxis, xAxis, charSpacing; /**< Render position and inter-character spacing. */
    uint8_t *fontMain;                  /**< Active font table pointer. */
    int16_t  x1Seg, y1Seg, originX1Seg, originY1Seg; /**< Segment bounding box. */
    uint8_t  textStyle;                 /**< FIXED_TEXT_STYLE or SCROLL_TEXT_STYLE. */
    uint16_t textHorizSpace = 0;        /**< Extra horizontal padding on the right of the rendered string. */
    uint16_t color;                     /**< Text foreground colour (RGB565). */
    uint16_t backgroundColor = BLACK;   /**< Background fill colour behind the text. */

    /** @brief One-time panel pin configuration — call once at startup. */
    static void mtb_Config_Disp_Panel_Pins(void);
    /** @brief Initialise the HUB75 DMA driver — call once at startup after pin config. */
    static void mtb_Init_Led_Matrix_Panel();
    /** @brief Clear the entire panel to black. */
    static void mtb_Clear_Screen(void);
    /** @brief Switch the active font. @param font Pointer to a GLCD font table. */
    void setfont(const uint8_t *font);
    /** @brief Draw a single character glyph. @param a Character code. @param x Pixel X. @param y Pixel Y. */
    void writeXter(uint16_t a, int16_t x, int16_t y);

    /**
     * @brief Render a C string at the configured position using the active font.
     * @param myString  Null-terminated character array.
     * @return Pixel width of the rendered string.
     */
    virtual uint16_t mtb_Write_String(const char *myString);

    /**
     * @brief Render an Arduino String at the configured position.
     * @param myString  Source String object.
     * @return Pixel width of the rendered string.
     */
    virtual uint16_t mtb_Write_String(String myString);

    virtual void mtb_Set_Pixel_Data(int16_t, int16_t) {}
    virtual void mtb_Update_Panel_Segment(void) {}
    virtual void mtb_Clear_Panel_Segment(void) {}

    Mtb_Static_Text_t();
    /**
     * @brief Construct a text renderer at the given position with a font.
     * @param x1    X pixel position.
     * @param y1    Y pixel position.
     * @param font  GLCD font table (default Terminal6x8).
     */
    Mtb_Static_Text_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8);

    virtual ~Mtb_Static_Text_t() {}
};

/**
 * @brief Fixed-position text widget that updates in place on the panel.
 *
 * Inherits from Mtb_Static_Text_t. On each write, the previous text region
 * is erased (filled with backgroundColor) before the new string is drawn.
 * PSRAM-allocated via overloaded new/delete.
 */
class Mtb_FixedText_t : public Mtb_Static_Text_t {
public:
    static uint8_t** scratchPad; /**< Shared scratch buffer used during glyph rendering. */

    virtual void mtb_Set_Pixel_Data(int16_t, int16_t) override;
    virtual void mtb_Update_Panel_Segment(void) override;
    virtual void mtb_Clear_Panel_Segment(void) override;

    virtual uint16_t mtb_Write_String(const char *myString) override { return Mtb_Static_Text_t::mtb_Write_String(myString); }
    virtual uint16_t mtb_Write_String(String myString) override { return Mtb_Static_Text_t::mtb_Write_String(myString); }

    /**
     * @brief Write a string with a custom colour (uses the object's background colour).
     * @param myString  Text to display.
     * @param dColor    Foreground colour (RGB565).
     * @return Pixel width of the rendered string.
     */
    virtual uint16_t mtb_Write_Colored_String(const char *myString, uint16_t dColor);

    /**
     * @brief Write a string with a custom foreground and background colour.
     * @param myString         Text to display.
     * @param dColor           Foreground colour (RGB565).
     * @param dBackgroundColor Background fill colour (RGB565).
     * @return Pixel width of the rendered string.
     */
    virtual uint16_t mtb_Write_Colored_String(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);

    /** @brief Write an Arduino String with a custom colour. */
    virtual uint16_t mtb_Write_Colored_String(String myString, uint16_t dColor);
    /** @brief Write an Arduino String with custom foreground and background colours. */
    virtual uint16_t mtb_Write_Colored_String(String myString, uint16_t dColor, uint16_t dBackgroundColor);

    /**
     * @brief Erase the current text by filling the region with backgroundColor.
     * @return Always 0.
     */
    virtual uint16_t mtb_Clear_String();

    Mtb_FixedText_t();
    /**
     * @brief Construct a fixed-position text widget.
     * @param x1              X pixel position.
     * @param y1              Y pixel position.
     * @param font            GLCD font table (default Terminal6x8).
     * @param dColor          Default text colour (default OLIVE_GREEN).
     * @param dBackGrndColor  Background fill colour (default BLACK).
     */
    Mtb_FixedText_t(uint16_t x1, uint16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) :
        Mtb_Static_Text_t(x1, y1, font) {
        textStyle = FIXED_TEXT_STYLE;
        color = dColor;
        backgroundColor = dBackGrndColor;
    }

    /** @brief Allocate the widget in PSRAM. */
    void *operator new(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    /** @brief Free a PSRAM-allocated widget. */
    void operator delete(void* ptr) { heap_caps_free(ptr); }

    virtual ~Mtb_FixedText_t() {}
};

/**
 * @brief Horizontally auto-centred variant of Mtb_FixedText_t.
 *
 * Computes the string pixel width and centres it within the panel's X axis
 * before delegating rendering to Mtb_FixedText_t. PSRAM-allocated.
 */
class Mtb_CentreText_t : public Mtb_FixedText_t {
public:
    uint16_t mtb_Write_String(const char *myString) override;
    uint16_t mtb_Write_String(String myString) override;

    uint16_t mtb_Write_Colored_String(const char *myString, uint16_t dColor);
    uint16_t mtb_Write_Colored_String(const char *myString, uint16_t dColor, uint16_t dBackgroundColor);
    uint16_t mtb_Write_Colored_String(String myString, uint16_t dColor);
    uint16_t mtb_Write_Colored_String(String myString, uint16_t dColor, uint16_t dBackgroundColor);

    void mtb_Update_Panel_Segment(void) override {}
    void mtb_Clear_Panel_Segment(void) override;

    Mtb_CentreText_t();
    /**
     * @brief Construct a centred text widget.
     * @param x1              Reference X position (used as the centring axis).
     * @param y1              Y pixel position.
     * @param font            GLCD font table (default Terminal6x8).
     * @param dColor          Default text colour (default OLIVE_GREEN).
     * @param dBackGrndColor  Background fill colour (default BLACK).
     */
    Mtb_CentreText_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8, uint16_t dColor = OLIVE_GREEN, uint16_t dBackGrndColor = BLACK) :
        Mtb_FixedText_t(x1, y1, font, dColor, dBackGrndColor) {}

    /** @brief Allocate the widget in PSRAM. */
    void *operator new(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    /** @brief Free a PSRAM-allocated widget. */
    void operator delete(void* ptr) { heap_caps_free(ptr); }
};

/**
 * @brief Internal helper class used by Mtb_ScrollText_t to render glyphs into a scroll pixel buffer.
 *
 * Overrides mtb_Set_Pixel_Data() to write into a 2D scrollBuffer instead of the panel directly.
 * Not intended for direct use — instantiated internally by the scroll subsystem.
 */
class ScrollTextHelper_t : public Mtb_Static_Text_t {
public:
    uint8_t **scrollBuffer; /**< Pointer to the 2D pixel buffer being populated. */
    virtual void mtb_Set_Pixel_Data(int16_t, int16_t) override;
    ScrollTextHelper_t();
    /**
     * @brief Construct a scroll helper targeting an existing pixel buffer.
     * @param x1    X start position.
     * @param y1    Y start position.
     * @param font  GLCD font table (default Terminal6x8).
     */
    ScrollTextHelper_t(int16_t x1, int16_t y1, const uint8_t *font = Terminal6x8) :
        Mtb_Static_Text_t(x1, y1, font) { textStyle = SCROLL_TEXT_STYLE; }
};

extern Hub75Driver* mtb_Display_Driver; /**< Global pointer to the active HUB75 DMA driver instance. */

#endif
