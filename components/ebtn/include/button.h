#ifndef _EBTN_BUTTON_H
#define _EBTN_BUTTON_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern QueueHandle_t buttonEvent_Q;
extern TaskHandle_t button_Task_H;
extern void button_Task(void *args);

/**
 * @brief Pin state callback prototype
 *
 * State logic can be inverted using the active_low bool.
 *
 * @param pin The GPIO pin to poll (can represent a virtual pin - e.g. I2C port expander pin)
 * @return Current state of pin (1;active/pressed, 0;inactive)
 */
typedef uint8_t (*ebtn_poll_state_cb_t)(gpio_num_t pin);

typedef void (*ebtn_prepoll_cb_t)();

typedef struct {
   gpio_num_t pin;
   ebtn_poll_state_cb_t poll_state_callback; // if NULL during init, uses builtin GPIO polling callback

   uint8_t group;

   bool internal_pull; // true to enable internal pullup/pulldowns (only if poll_state_callback was NULL during init)
   bool active_low;    // true if button is active low instead of active high

   void* ctx;

   struct {
      uint8_t state;
      uint32_t last_changed_ms;

      uint8_t click_count;
      bool long_press_pending;
      uint32_t previous_delta_ms;
   } internal;
} button_t;

typedef enum {
   BUTTON_RELEASED = 0, // Released
   BUTTON_PRESSED,      // Pressed
   BUTTON_PRESSED_LONG, // Long Press
   BUTTON_CLICKED,      // Clicked one or more times (see button_event_t#count)
} button_event_type_t;

typedef struct {
   button_t* sender; // button that sent this event
   button_event_type_t type;

   uint8_t count;
   uint32_t delta_ms; // time since last state change in milliseconds (e.g. released -> pressed, or pressed -> released), zero if not available/applicable
} button_event_t;

/**
 * @brief Init library
 *
 * Creates and starts button polling timer and mutex.
 *
 * @param queue Event queue to send button events into
 * @return ESP_OK on success
 */
esp_err_t button_init(QueueHandle_t queue);

/**
 * @brief Cleanup library
 *
 * Stops and deletes polling timer and mutex.
 *
 * @return ESP_OK on success
 */
esp_err_t button_free();

/**
 * @brief Sets the pre-poll callback used for buttons
 *
 * Called just before polling all buttons. Useful for applications such as I2C port expanders.
 *
 * @param prepoll_callback The prepoll callback function, NULL to disable
 */
void button_set_prepoll_callback(ebtn_prepoll_cb_t prepoll_callback);

/**
 * @brief Init and add the specified button to the polling loop
 *
 * Button isn't copied. Ensure button_remove() is used before destruction/deallocation.
 *
 * @param btn Pointer reference to the button
 *
 * @return ESP_OK on success
 */
esp_err_t button_add(button_t* btn);

/**
 * @brief Removes button from the polling loop
 *
 * @param btn Pointer reference to the button
 *
 * @return ESP_OK on success
 */
esp_err_t button_remove(button_t* btn);

#ifdef __cplusplus
}
#endif

#endif