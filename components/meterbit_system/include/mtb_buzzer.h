/**
 * @file mtb_buzzer.h
 * @brief Piezo buzzer beep patterns and driver API.
 *
 * Provides a FreeRTOS-based buzzer driver that drives the piezo element on
 * `ALM_BUZZER` (GPIO 48) via the LEDC PWM peripheral. Callers queue a beep
 * type and count using `mtb_Do_Beep()`; the `buzzer_Beep` task pulls requests
 * from the semaphore and executes the tone pattern asynchronously.
 */
#ifndef BEEP_H
#define BEEP_H

/**
 * @brief Predefined beep pattern identifiers.
 */
enum BEEPING {
    BEEP_0 = 1, /**< Short single-frequency pip. */
    BEEP_1,     /**< Medium single-frequency beep. */
    BEEP_2,     /**< Double-pip pattern. */
    BEEP_3,     /**< Triple-pip pattern. */
    BEEP_4,     /**< Long continuous tone. */
    CLICK_BEEP  /**< Brief click (UI feedback for encoder presses). */
};

extern SemaphoreHandle_t beep_Duration_Sem; /**< Binary semaphore given by mtb_Do_Beep() to wake the buzzer task. */
extern TaskHandle_t      beepTaskHandle;     /**< Task handle for the `buzzer_Beep` driver task. */

/**
 * @brief Request a beep pattern from any task or ISR context.
 *
 * Sets the beep type and repeat count, then gives `beep_Duration_Sem` to
 * unblock the `buzzer_Beep` task. Safe to call from ISR context.
 *
 * @param beep_Type   Beep pattern to play (one of the `BEEPING` enum values).
 * @param beep_Count  Number of times to repeat the pattern (default 1).
 */
extern void mtb_Do_Beep(uint8_t beep_Type, uint8_t beep_Count = 1);

/**
 * @brief FreeRTOS task entry point for the buzzer driver.
 *
 * Blocks on `beep_Duration_Sem` and plays the requested beep pattern each time
 * the semaphore is taken. Runs for the lifetime of the firmware.
 *
 * @param arguments  Unused task parameter.
 */
extern void buzzer_Beep(void *arguments);

#endif
