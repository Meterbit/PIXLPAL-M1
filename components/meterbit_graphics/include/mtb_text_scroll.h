/**
 * @file mtb_text_scroll.h
 * @brief Horizontally scrolling text widget and task API.
 *
 * Provides Mtb_ScrollText_t, a PSRAM-allocated text object that scrolls a
 * character string across a user-defined region of the HUB75 panel. Scrolling
 * is driven by one of a pool of FreeRTOS tasks (scroll_Q[], scrollText_Handles[]).
 * A convenience global, statusBarNotif, is pre-wired to the status-bar row and is
 * used throughout the firmware to show event notifications to the user.
 *
 * Call mtb_Text_Scrolls_Init() once during system boot to allocate the scroll
 * task pool before any application tries to scroll text.
 */
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

/** @defgroup mtb_scroll_ctrl Scroll Control Constants
 * @{ */
#define CHECK_SCROLL  0   /**< Passed to mtb_Scroll_Active(): query whether the scroll is still running. */
#define STOP_SCROLL   1   /**< Passed to mtb_Scroll_Active(): stop the current scroll immediately. */
#define CHAR_LEN      1005 /**< Maximum character capacity of the pixel buffer per scroll object. */
#define DEF_SPEED     25   /**< Default scroll speed (arbitrary unit; smaller = faster). */
#define SCROLL_HOLD   1    /**< Value used for scroll_Hold to pause the scroll at the end of the string. */
/** @} */

/* ---- Shared scroll task handles and queues ---- */
extern TaskHandle_t scrollText_Handles[]; /**< Array of task handles for the scroll task pool. */
extern QueueHandle_t scroll_Q[];           /**< Array of queues feeding each scroll task with text requests. */
extern TaskHandle_t scrollQ_Handle;        /**< Handle of the scroll queue dispatcher task. */
extern uint8_t taskState;                  /**< Bitmask tracking which scroll tasks are currently busy. */
extern TaskHandle_t message_Scroll;        /**< Task handle for the single-instance status-bar scroll task. */

extern uint8_t stopCurrentScroll;  /**< Set to pdTRUE to abort the currently active scroll on the next frame. */
extern uint16_t scrollLength;      /**< Pixel width of the scroll region across the X axis. */
extern uint16_t scrollSpeed;       /**< Active scroll step rate in pixels per tick. */
extern int16_t scrollCount;        /**< Remaining pixel steps in the current scroll pass. */

/**
 * @brief PSRAM-allocated scrolling text object.
 *
 * Defines the text region, font, color, speed, and number of passes for a
 * horizontal scroll animation. Call one of the mtb_Scroll_This_Text() overloads
 * to enqueue a scroll request on an idle task in the pool.
 *
 * All instances are PSRAM-allocated via overloaded new/delete. The == operator
 * compares by (xPos, yPos) to detect duplicate scroll regions.
 */
class Mtb_ScrollText_t {
public:
    uint16_t xPos;            /**< X-axis start position of the scroll region in pixels. */
    uint16_t yPos;            /**< Y-axis row of the scroll region in pixels. */
    uint16_t width;           /**< Pixel width of the scroll region (clipping boundary). */
    uint16_t height;          /**< Pixel height of the scroll region. */
    uint16_t color;           /**< Default text colour (RGB565). */
    uint16_t backgroundColor = BLACK; /**< Background fill colour cleared before each frame. */
    uint16_t speed;           /**< Scroll speed (pixels per task tick; default DEF_SPEED). */
    uint16_t pass;            /**< Number of times the text string is scrolled through (1 = once). */
    uint8_t *font;            /**< Pointer to the MikroElektronika GLCD font table to use. */
    uint8_t** dText_Raw;      /**< Pointer to the rendered pixel buffer for the current text. */
    uint64_t span;            /**< Total pixel width of the rendered text string. */
    uint64_t stretch;         /**< Internal stretch factor for the pixel buffer. */
    uint64_t scroll_Hold = pdFALSE; /**< pdTRUE to hold the last frame visible when the scroll finishes. */
    uint8_t scroll_Quit = pdFALSE; /**< pdTRUE to signal the running task to abort this scroll. */
    uint8_t beep;             /**< pdTRUE to emit a brief beep when the scroll completes. */

    uint8_t scrollTaskHandling = 0; /**< Index of the pool task currently handling this object's scroll. */
    static Mtb_ScrollText_t *scrollTask_HolderPointers[]; /**< Array mapping pool task indices to their active scroll objects. */

    /**
     * @brief Start a scroll using the provided task handle directly.
     * @param taskHandle  Reference to the TaskHandle_t of the pool task to use.
     */
    void mtb_Scroll_String(TaskHandle_t& taskHandle);

    /**
     * @brief Scroll a null-terminated C string using default colour and speed.
     * @param text  Text to scroll.
     */
    void mtb_Scroll_This_Text(const char* text);

    /**
     * @brief Scroll a C string with a custom colour.
     * @param text   Text to scroll.
     * @param c      Text colour (RGB565).
     */
    void mtb_Scroll_This_Text(const char* text, uint16_t c);

    /**
     * @brief Scroll a C string with a custom colour and pass count.
     * @param text   Text to scroll.
     * @param c      Text colour (RGB565).
     * @param p      Number of passes (loops) through the text.
     */
    void mtb_Scroll_This_Text(const char* text, uint16_t c, uint16_t p);

    /**
     * @brief Scroll a C string with colour, pass count, and optional beep.
     * @param text   Text to scroll.
     * @param c      Text colour (RGB565).
     * @param p      Number of passes.
     * @param b      pdTRUE to beep on scroll completion.
     */
    void mtb_Scroll_This_Text(const char* text, uint16_t c, uint16_t p, uint8_t b);

    /** @brief Scroll an Arduino String using default colour and speed. */
    void mtb_Scroll_This_Text(String dText);

    /** @brief Scroll an Arduino String with a custom colour. */
    void mtb_Scroll_This_Text(String dText, uint16_t c);

    /** @brief Scroll an Arduino String with colour and pass count. */
    void mtb_Scroll_This_Text(String dText, uint16_t c, uint16_t p);

    /** @brief Scroll an Arduino String with colour, pass count, and optional beep. */
    void mtb_Scroll_This_Text(String dText, uint16_t c, uint16_t p, uint8_t b);

    /**
     * @brief Query or stop the active scroll on this object.
     * @param check_or_stop  CHECK_SCROLL (default) to query, STOP_SCROLL to abort.
     * @return pdTRUE if a scroll is currently active, pdFALSE otherwise.
     */
    uint8_t mtb_Scroll_Active(uint8_t check_or_stop = CHECK_SCROLL);

    /** @brief Allocate the scroll object in PSRAM. */
    void *operator new(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    /** @brief Free a PSRAM-allocated scroll object. */
    void operator delete(void* ptr) { heap_caps_free(ptr); }

    /** @brief Compare two scroll objects by region position. */
    bool operator==(const Mtb_ScrollText_t &other) const {
        return (xPos == other.xPos) && (yPos == other.yPos);
    }

    Mtb_ScrollText_t() {}

    /**
     * @brief Construct a fully-configured scroll text object.
     * @param xAxis     Left edge of the scroll region in pixels.
     * @param yAxis     Top edge of the scroll region in pixels.
     * @param width     Pixel width of the scroll region.
     * @param font      MikroElektronika GLCD font table pointer.
     * @param color     Text colour (RGB565, default WHITE).
     * @param speed     Scroll speed (default DEF_SPEED).
     * @param pass      Number of passes (default 1).
     * @param scr_hld   pdTRUE to hold the last frame when scroll ends (default 0).
     * @param makeBeep  pdTRUE to beep on completion (default 0).
     */
    Mtb_ScrollText_t(uint16_t xAxis, uint16_t yAxis, uint16_t width, const uint8_t* font, uint16_t color = WHITE, uint16_t speed = DEF_SPEED, uint16_t pass = 1, uint64_t scr_hld = 0, uint8_t makeBeep = 0);

    /**
     * @brief Construct a minimal scroll object with colour, pass count, and optional beep.
     * @param color     Text colour (RGB565).
     * @param pass      Number of passes (default 1).
     * @param makeBeep  pdTRUE to beep on completion (default 0).
     */
    Mtb_ScrollText_t(uint16_t color, uint16_t pass = 1, uint8_t makeBeep = 0);
};

/**
 * @brief Initialise the scroll task pool and allocate pixel buffers.
 * @note  Must be called once before any application enqueues a scroll request.
 */
extern void mtb_Text_Scrolls_Init(void);

/**
 * @brief FreeRTOS task entry point for scroll task slot 0 (the status-bar notifier slot).
 * @param arguments  Unused task parameter.
 */
void mtb_Scroll_Text_0_Task(void *arguments);

extern Mtb_ScrollText_t statusBarNotif; /**< Global pre-configured scroll object for the status-bar notification row. */

#endif
