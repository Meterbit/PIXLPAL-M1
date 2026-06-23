/**
 * @file mtb_PosixTZtoLocalTime.h
 * @brief POSIX Timezone String Parser and Local Time Calculator
 * 
 * Parses POSIX TZ strings and calculates local time without using setenv().
 * Supports both simple offsets (e.g., "JST-9") and full DST rules
 * (e.g., "EST5EDT,M3.2.0/2,M11.1.0/2").
 * 
 * @author Pixlpal Project
 * @version 1.0.0
 */

#ifndef TIMEZONE_PARSER_H
#define TIMEZONE_PARSER_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#define MAX_WORLD_CLOCKS    5   // Maximum number of cached timezones
#define TZ_STRING_MAX_LEN   64  // Maximum length of TZ string
#define TZ_NAME_MAX_LEN     8   // Maximum length of timezone abbreviation

/*******************************************************************************
 * Data Types
 ******************************************************************************/

/**
 * @brief Timezone information structure
 * 
 * Contains parsed timezone data including offsets and DST rules.
 */
typedef struct {
    char tzString[TZ_STRING_MAX_LEN];   ///< Original TZ string
    char stdName[TZ_NAME_MAX_LEN];      ///< Standard time abbreviation (e.g., "EST")
    char dstName[TZ_NAME_MAX_LEN];      ///< DST abbreviation (e.g., "EDT")
    long stdOffset;                      ///< Standard time offset in seconds from UTC
    long dstOffset;                      ///< DST offset in seconds from UTC
    bool hasDST;                         ///< Whether DST rules exist
    
    // DST start transition rule
    uint8_t dstStartMonth;              ///< Month (1-12)
    uint8_t dstStartWeek;               ///< Week (1-5, where 5 = last)
    uint8_t dstStartDay;                ///< Day of week (0-6, 0 = Sunday)
    int     dstStartHour;               ///< Hour of transition (default 2)
    
    // DST end transition rule
    uint8_t dstEndMonth;                ///< Month (1-12)
    uint8_t dstEndWeek;                 ///< Week (1-5, where 5 = last)
    uint8_t dstEndDay;                  ///< Day of week (0-6, 0 = Sunday)
    int     dstEndHour;                 ///< Hour of transition (default 2)
} TimezoneInfo;

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

/**
 * @brief Parse a POSIX timezone string
 * 
 * Parses timezone strings in the following formats:
 * - "STDoffset" (e.g., "JST-9")
 * - "STDoffsetDST" (e.g., "EST5EDT")
 * - "STDoffsetDST,start_rule,end_rule" (e.g., "EST5EDT,M3.2.0/2,M11.1.0/2")
 * 
 * @param tzString  POSIX timezone string to parse
 * @param tz        Pointer to TimezoneInfo structure to populate
 * @return true     Parsing successful
 * @return false    Parsing failed (invalid format)
 */
bool parseTimezone(const char* tzString, TimezoneInfo* tz);

/**
 * @brief Cache a timezone for repeated use
 * 
 * Parses and stores a timezone in the internal cache. Call this once
 * at startup for each timezone you need.
 * 
 * @param tzString  POSIX timezone string to cache
 * @return int      Cache index (0 to MAX_WORLD_CLOCKS-1) on success, -1 on failure
 */
int cacheTimezone(const char* tzString);

/**
 * @brief Clear all cached timezones
 * 
 * Resets the timezone cache. Call before re-initializing timezones.
 */
void clearTimezoneCache(void);

/**
 * @brief Get the number of cached timezones
 * 
 * @return uint8_t  Number of timezones currently cached
 */
uint8_t getCachedTimezoneCount(void);

/**
 * @brief Get cached timezone info by index
 * 
 * @param index     Cache index (0 to getCachedTimezoneCount()-1)
 * @return const TimezoneInfo*  Pointer to timezone info, or NULL if invalid index
 */
const TimezoneInfo* getCachedTimezone(uint8_t index);

/**
 * @brief Get current UTC offset for a timezone
 * 
 * Returns the appropriate offset based on whether DST is currently active.
 * 
 * @param tz        Pointer to TimezoneInfo structure
 * @param utcTime   Current UTC time
 * @return long     Offset in seconds from UTC
 */
long getCurrentOffset(const TimezoneInfo* tz, time_t utcTime);

/**
 * @brief Check if DST is currently active
 * 
 * @param tz        Pointer to TimezoneInfo structure
 * @param utcTime   Current UTC time
 * @return true     DST is active
 * @return false    DST is not active (or timezone has no DST)
 */
bool isDstActive(const TimezoneInfo* tz, time_t utcTime);

/**
 * @brief Get formatted local time string for a timezone
 * 
 * Returns time in "HH:MM" format. Uses caching to only update the buffer
 * when time changes.
 * 
 * @param tzString        POSIX timezone string (must be previously cached)
 * @param timeTextBuffer  Buffer to store result (minimum 6 bytes: "HH:MM\0")
 * @return char*          Pointer to timeTextBuffer
 * 
 * @note The timezone must be cached using cacheTimezone() before calling this function.
 */
char* getCityLocalTime(const char* tzString, char* timeTextBuffer);

/**
 * @brief Get full local time structure for a timezone
 * 
 * @param tzString  POSIX timezone string (must be previously cached)
 * @param tmOut     Pointer to tm structure to populate
 * @return true     Success
 * @return false    Timezone not found in cache
 */
bool getLocalTm(const char* tzString, struct tm* tmOut);

#ifdef __cplusplus
}
#endif

#endif /* TIMEZONE_PARSER_H */