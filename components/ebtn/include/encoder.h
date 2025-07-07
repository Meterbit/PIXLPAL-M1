#ifndef _EBTN_ENCODER_H
#define _EBTN_ENCODER_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include "button.h"

#ifdef __cplusplus
extern "C" {
#endif

extern QueueHandle_t encoderEvent_Q;
extern TaskHandle_t encoder_Task_H;
extern void encoder_Task(void *args);


typedef struct {
   button_t* btn; // not currently used, set NULL if no button
   gpio_num_t pin_a;
   gpio_num_t pin_b;

   ebtn_poll_state_cb_t poll_state_callback; // if NULL during init, uses builtin GPIO polling callback

   bool internal_pull; // true to enable internal pullup/pulldowns (only if poll_state_callback was NULL during init)
   bool active_low;    // true if encoder pins are active low instead of active high

   void* ctx;

   struct {
      uint8_t code;
      uint16_t store;
   } internal;

} rotary_encoder_t;

typedef enum {
   ROT_CLOCKWISE = 0,
   ROT_COUNTERCLOCKWISE = 1,
} rotary_encoder_rotation_t;

typedef struct {
   rotary_encoder_t* sender;      // rotary encoder that sent this event
   rotary_encoder_rotation_t dir; // direction of rotation (-1;counterclockwise, 1;clockwise)
} rotary_encoder_event_t;

/**
 * @brief Init library
 *
 * Creates and starts button polling timer and mutex.
 *
 * @param queue Event queue to send encoder events into
 * @return ESP_OK on success
 */
esp_err_t rotary_encoder_init(QueueHandle_t queue);

/**
 * @brief Cleanup library
 *
 * Stops and deletes polling timer and mutex.
 *
 * @return ESP_OK on success
 */
esp_err_t rotary_encoder_free();

/**
 * @brief Sets the pre-poll callback used for rotary encoders
 *
 * Called just before polling all rotary encoders. Useful for applications such as I2C port expanders.
 *
 * @param prepoll_callback The prepoll callback function, NULL to disable
 */
void rotary_encoder_set_prepoll_callback(ebtn_prepoll_cb_t prepoll_callback);

/**
 * @brief Init and add the specified encoder to the polling loop
 *
 * Encoder isn't copied. Ensure rotary_encoder_remove() is used before destruction/deallocation.
 *
 * @param btn Pointer reference to the encoder
 *
 * @return ESP_OK on success
 */
esp_err_t rotary_encoder_add(rotary_encoder_t* enc);

/**
 * @brief Removes encoder from the polling loop
 *
 * @param btn Pointer reference to the encoder
 *
 * @return ESP_OK on success
 */
esp_err_t rotary_encoder_remove(rotary_encoder_t* enc);

#ifdef __cplusplus
}
#endif

#endif