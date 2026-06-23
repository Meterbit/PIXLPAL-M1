/**
 * @file mtb_ntp.h
 * @brief SNTP time synchronisation task and calendar enumerations.
 *
 * Provides `sntp_Time_init_Task`, a FreeRTOS task that configures the SNTP client,
 * waits for the first successful time synchronisation from an NTP server, and then
 * self-deletes. After synchronisation, standard C `time()` / `localtime()` calls
 * return valid wall-clock time using the TZ string loaded from NVS (`ntp_TimeZone`).
 */
#ifndef NTP_TIME_H
#define NTP_TIME_H

#define RESTART 0; /**< Sentinel passed to esp_restart() — included for legacy compatibility. */

#include "mtb_graphics.h"

extern TaskHandle_t sntp_Time_handle; /**< Task handle for the SNTP sync task; NULL after self-deletion. */

/**
 * @brief FreeRTOS task that initialises and synchronises SNTP time.
 *
 * Configures the SNTP client with the server address stored in NVS, polls until
 * the RTC is updated (year > 2023), applies the `ntp_TimeZone` POSIX TZ string,
 * and self-deletes.
 *
 * @param params  Unused task parameter.
 */
extern void sntp_Time_init_Task(void *params);

/**
 * @brief Day-of-week indices returned by `struct tm::tm_wday`.
 */
enum WEEKDAYS {
    SUN = 0, /**< Sunday */
    MON,     /**< Monday */
    TUE,     /**< Tuesday */
    WED,     /**< Wednesday */
    THU,     /**< Thursday */
    FRI,     /**< Friday */
    SAT      /**< Saturday */
};

/**
 * @brief Month indices returned by `struct tm::tm_mon` (0-based).
 */
enum MONTHS {
    JANUARY = 0, /**< January (0) */
    FEBUARY,     /**< February (1) */
    MARCH,       /**< March (2) */
    APRIL,       /**< April (3) */
    MAY,         /**< May (4) */
    JUNE,        /**< June (5) */
    JULY,        /**< July (6) */
    AUGUST,      /**< August (7) */
    SEPTEMBER,   /**< September (8) */
    OCTOBER,     /**< October (9) */
    NOVEMBER,    /**< November (10) */
    DECEMBER     /**< December (11) */
};

#endif
