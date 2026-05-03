/**
 * @file TimezoneParser.cpp
 * @brief POSIX Timezone String Parser and Local Time Calculator (NO setenv/tzset, NO leaks)
 *
 * Now supports POSIX DST rule formats:
 *   - M.m.w[/time]   (month.week.day)
 *   - Jn[/time]      (Julian day 1..365, ignores Feb 29)
 *   - n[/time]       (day of year 0..365, includes Feb 29)
 *
 * IMPORTANT NOTE (because your TimezoneInfo struct has no explicit “rule type” fields):
 * - For M rules, we store month/week/day normally (month=1..12).
 * - For Jn rules, we store:
 *     month = 13
 *     week  = (dayOfYear >> 8)
 *     day   = (dayOfYear & 0xFF)
 *   where dayOfYear is the "n" in Jn (1..365).
 * - For n rules, we store:
 *     month = 0
 *     week  = (dayOfYear >> 8)
 *     day   = (dayOfYear & 0xFF)
 *   where dayOfYear is 0..365.
 *
 * Transition time (/time):
 * - Your TimezoneInfo stores only hour. If /time includes non-zero minutes/seconds, parsing fails.
 */

#include "mtb_PosixTZtoLocalTime.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/*******************************************************************************
 * Private Variables
 ******************************************************************************/

EXT_RAM_BSS_ATTR TimezoneInfo tzCache[MAX_WORLD_CLOCKS];

static uint8_t tzCacheCount = 0;

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/

static long  parseOffset(const char* str, int* charsConsumed);
static int   parseTzName(const char* str, char* name, int maxLen);
static int   parseDstRule(const char* str, uint8_t* month, uint8_t* week,
                          uint8_t* day, int* hour);

static int   findDayOfMonth(int year, int month, int dayOfWeek, int week);
static int   findTimezone(const char* tzString);

/* "UTC mktime" without TZ/env/tzset */
static time_t mktime_utc_local(const struct tm* t);

/* Helpers for Jn / n rules */
static bool  is_leap(int y);
static int   days_in_month(int y, int m);
static int   days_before_year(int y);
static int   days_before_month(int y, int m);
static int   doy_to_month_day(int year, int doy0_based, int* outMonth, int* outMday);
static uint16_t getPackedDoy(uint8_t week, uint8_t day);

/*******************************************************************************
 * Private Helpers (no TZ env usage)
 ******************************************************************************/

static bool is_leap(int y) {
    return ((y % 4) == 0) && (((y % 100) != 0) || ((y % 400) == 0));
}

static int days_in_month(int y, int m) {
    static const int dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return dim[m - 1] + (is_leap(y) ? 1 : 0);
    return dim[m - 1];
}

static int days_before_year(int y) {
    // days from 1970-01-01 to y-01-01 (y full year), y >= 1970
    int years = y - 1970;

    int a = (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    int b = 1969 / 4 - 1969 / 100 + 1969 / 400;

    int leaps = a - b;
    return years * 365 + leaps;
}

static int days_before_month(int y, int m) {
    static const int cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int d = cum[m - 1];
    if (m > 2 && is_leap(y)) d += 1;
    return d;
}

/**
 * Convert a UTC broken-down struct tm (treated as UTC) to epoch seconds.
 * No timezone/dst assumptions, no libc TZ state.
 */
static time_t mktime_utc_local(const struct tm* t) {
    if (!t) return (time_t)-1;

    int year = t->tm_year + 1900;
    int mon  = t->tm_mon + 1;     // 1..12
    int mday = t->tm_mday;        // 1..31

    if (year < 1970 || mon < 1 || mon > 12 || mday < 1 || mday > 31) return (time_t)-1;
    int dim = days_in_month(year, mon);
    if (mday > dim) return (time_t)-1;

    long days = (long)days_before_year(year) +
                (long)days_before_month(year, mon) +
                (long)(mday - 1);

    long secs = days * 86400L +
                (long)t->tm_hour * 3600L +
                (long)t->tm_min * 60L +
                (long)t->tm_sec;

    return (time_t)secs;
}

static uint16_t getPackedDoy(uint8_t week, uint8_t day) {
    return (uint16_t)(((uint16_t)week << 8) | (uint16_t)day);
}

/**
 * Convert 0-based day-of-year (0..365) into month (1..12) and mday (1..31).
 */
static int doy_to_month_day(int year, int doy0_based, int* outMonth, int* outMday) {
    if (!outMonth || !outMday) return 0;

    int maxDoy = is_leap(year) ? 365 : 364;
    if (doy0_based < 0 || doy0_based > maxDoy) return 0;

    int m = 1;
    int d = doy0_based + 1; // make 1-based day counter for subtraction
    while (m <= 12) {
        int dim = days_in_month(year, m);
        if (d <= dim) break;
        d -= dim;
        m++;
    }
    if (m > 12) return 0;

    *outMonth = m;
    *outMday  = d;
    return 1;
}

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

static long parseOffset(const char* str, int* charsConsumed) {
    if (charsConsumed) *charsConsumed = 0;
    if (!str) return 0;

    long hours = 0, minutes = 0, seconds = 0;
    int sign = 1;
    int pos = 0;

    if (str[pos] == '-') { sign = -1; pos++; }
    else if (str[pos] == '+') { pos++; }

    if (!isdigit((unsigned char)str[pos])) return 0;

    while (isdigit((unsigned char)str[pos])) {
        hours = hours * 10 + (str[pos] - '0');
        pos++;
    }

    if (str[pos] == ':') {
        pos++;
        if (!isdigit((unsigned char)str[pos])) return 0;

        while (isdigit((unsigned char)str[pos])) {
            minutes = minutes * 10 + (str[pos] - '0');
            pos++;
        }
        if (minutes > 59) return 0;

        if (str[pos] == ':') {
            pos++;
            if (!isdigit((unsigned char)str[pos])) return 0;

            while (isdigit((unsigned char)str[pos])) {
                seconds = seconds * 10 + (str[pos] - '0');
                pos++;
            }
            if (seconds > 59) return 0;
        }
    }

    if (charsConsumed) *charsConsumed = pos;

    long posix = sign * (hours * 3600L + minutes * 60L + seconds);
    return -posix;
}

static int parseTzName(const char* str, char* name, int maxLen) {
    if (!str || !name || maxLen <= 1) return 0;

    int pos = 0;
    int out = 0;

    if (str[pos] == '<') {
        pos++;
        while (str[pos] && str[pos] != '>' && out < (maxLen - 1)) {
            name[out++] = str[pos++];
        }
        if (str[pos] != '>') return 0;
        pos++;

        if (out < 1) return 0;
        name[out] = '\0';
        return pos;
    }

    while (isalpha((unsigned char)str[pos]) && out < (maxLen - 1)) {
        name[out++] = str[pos++];
    }
    if (out < 3) return 0;

    name[out] = '\0';
    return pos;
}

/**
 * Parse DST rule:
 *   - M.m.w[/time]
 *   - Jn[/time]     (1..365)
 *   - n[/time]      (0..365)
 *
 * Storage encoding (because TimezoneInfo has only month/week/day/hour):
 *   - M: month=1..12, week=1..5, day=0..6
 *   - Jn: month=13, week/day store packed n (1..365)
 *   - n: month=0,  week/day store packed n (0..365)
 */
static int parseDstRule(const char* str, uint8_t* month, uint8_t* week,
                        uint8_t* day, int* hour) {
    if (!str || !month || !week || !day || !hour) return 0;

    int pos = 0;

    // Default transition time is 02:00
    *hour = 2;

    // ---- M.m.w ----
    if (str[pos] == 'M') {
        pos++;

        int m = 0;
        if (!isdigit((unsigned char)str[pos])) return 0;
        while (isdigit((unsigned char)str[pos])) {
            m = m * 10 + (str[pos] - '0');
            pos++;
        }
        if (m < 1 || m > 12) return 0;

        if (str[pos] != '.') return 0;
        pos++;

        if (!isdigit((unsigned char)str[pos])) return 0;
        int w = str[pos] - '0';
        pos++;
        if (w < 1 || w > 5) return 0;

        if (str[pos] != '.') return 0;
        pos++;

        if (!isdigit((unsigned char)str[pos])) return 0;
        int d = str[pos] - '0';
        pos++;
        if (d < 0 || d > 6) return 0;

        *month = (uint8_t)m;
        *week  = (uint8_t)w;
        *day   = (uint8_t)d;
    }
    // ---- Jn ----
    else if (str[pos] == 'J') {
        pos++;

        int n = 0;
        if (!isdigit((unsigned char)str[pos])) return 0;
        while (isdigit((unsigned char)str[pos])) {
            n = n * 10 + (str[pos] - '0');
            pos++;
        }
        if (n < 1 || n > 365) return 0;

        *month = 13; // sentinel for Jn
        *week  = (uint8_t)((n >> 8) & 0xFF);
        *day   = (uint8_t)(n & 0xFF);
    }
    // ---- n ---- (starts with digit)
    else if (isdigit((unsigned char)str[pos])) {
        int n = 0;
        while (isdigit((unsigned char)str[pos])) {
            n = n * 10 + (str[pos] - '0');
            pos++;
        }
        if (n < 0 || n > 365) return 0;

        *month = 0; // sentinel for n
        *week  = (uint8_t)((n >> 8) & 0xFF);
        *day   = (uint8_t)(n & 0xFF);
    }
    else {
        return 0; // unknown rule type
    }

    // Optional /time
    if (str[pos] == '/') {
        pos++;

        int sign = 1;
        if (str[pos] == '-') { sign = -1; pos++; }
        else if (str[pos] == '+') { pos++; }

        if (!isdigit((unsigned char)str[pos])) return 0;

        long hh = 0, mm = 0, ss = 0;
        while (isdigit((unsigned char)str[pos])) {
            hh = hh * 10 + (str[pos] - '0');
            pos++;
        }

        if (str[pos] == ':') {
            pos++;
            if (!isdigit((unsigned char)str[pos])) return 0;
            while (isdigit((unsigned char)str[pos])) {
                mm = mm * 10 + (str[pos] - '0');
                pos++;
            }
            if (mm > 59) return 0;

            if (str[pos] == ':') {
                pos++;
                if (!isdigit((unsigned char)str[pos])) return 0;
                while (isdigit((unsigned char)str[pos])) {
                    ss = ss * 10 + (str[pos] - '0');
                    pos++;
                }
                if (ss > 59) return 0;
            }
        }

        // We only store hours -> reject non-zero minutes/seconds
        if (mm != 0 || ss != 0) return 0;

        *hour = (int)(sign * hh);
    }

    return pos;
}

static int findDayOfMonth(int year, int month, int dayOfWeek, int week) {
    // weekday of 1970-01-01 was Thursday => 4
    long days = (long)days_before_year(year) + (long)days_before_month(year, month);
    int first_wday = (int)((4 + days) % 7);
    if (first_wday < 0) first_wday += 7;

    int firstOccurrence = 1 + (dayOfWeek - first_wday + 7) % 7;

    if (week == 5) {
        int dim = days_in_month(year, month);
        int d = firstOccurrence;
        while (d + 7 <= dim) d += 7;
        return d;
    }

    return firstOccurrence + (week - 1) * 7;
}

static int findTimezone(const char* tzString) {
    if (!tzString) return -1;

    for (int i = 0; i < (int)tzCacheCount; i++) {
        if (strcmp(tzCache[i].tzString, tzString) == 0) {
            return i;
        }
    }
    return -1;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

bool parseTimezone(const char* tzString, TimezoneInfo* tz) {
    if (!tzString || !tz) return false;

    memset(tz, 0, sizeof(TimezoneInfo));
    strncpy(tz->tzString, tzString, sizeof(tz->tzString) - 1);
    tz->tzString[sizeof(tz->tzString) - 1] = '\0';

    const char* ptr = tzString;
    int consumed = 0;

    consumed = parseTzName(ptr, tz->stdName, (int)sizeof(tz->stdName));
    if (consumed == 0) return false;
    ptr += consumed;

    tz->stdOffset = parseOffset(ptr, &consumed);
    if (consumed == 0) return false;
    ptr += consumed;

    if (*ptr == '\0') {
        tz->hasDST = false;
        return true;
    }

    consumed = parseTzName(ptr, tz->dstName, (int)sizeof(tz->dstName));
    if (consumed == 0) return false;
    ptr += consumed;

    if (*ptr == ',' || *ptr == '\0') {
        tz->dstOffset = tz->stdOffset + 3600;
    } else {
        tz->dstOffset = parseOffset(ptr, &consumed);
        if (consumed == 0) return false;
        ptr += consumed;
    }

    if (*ptr == '\0') {
        tz->hasDST = true;
        return true;
    }

    if (*ptr != ',') return false;
    ptr++;

    consumed = parseDstRule(ptr, &tz->dstStartMonth, &tz->dstStartWeek,
                           &tz->dstStartDay, &tz->dstStartHour);
    if (consumed == 0) return false;
    ptr += consumed;

    if (*ptr != ',') return false;
    ptr++;

    consumed = parseDstRule(ptr, &tz->dstEndMonth, &tz->dstEndWeek,
                           &tz->dstEndDay, &tz->dstEndHour);
    if (consumed == 0) return false;
    ptr += consumed;

    if (*ptr != '\0') return false;

    tz->hasDST = true;
    return true;
}

int cacheTimezone(const char* tzString) {
    if (!tzString) return -1;
    if (tzCacheCount >= MAX_WORLD_CLOCKS) return -1;

    int existing = findTimezone(tzString);
    if (existing >= 0) return existing;

    if (!parseTimezone(tzString, &tzCache[tzCacheCount])) {
        return -1;
    }

    int insertedIndex = (int)tzCacheCount;
    tzCacheCount++;
    return insertedIndex;
}

void clearTimezoneCache(void) {
    tzCacheCount = 0;
    memset(tzCache, 0, sizeof(tzCache));
}

uint8_t getCachedTimezoneCount(void) {
    return tzCacheCount;
}

const TimezoneInfo* getCachedTimezone(uint8_t index) {
    if (index >= tzCacheCount) return NULL;
    return &tzCache[index];
}

/**
 * Build UTC epoch timestamp for a transition rule in a given year.
 * The rule time is expressed as LOCAL time.
 * Returns 1 on success and fills outUtc.
 */
static int buildTransitionUtc(const TimezoneInfo* tz,
                              int year,
                              uint8_t ruleMonth,
                              uint8_t ruleWeek,
                              uint8_t ruleDay,
                              int ruleHour,
                              long offsetUsedToConvertLocalToUtc,
                              time_t* outUtc)
{
    if (!tz || !outUtc) return 0;

    int month = 0;
    int mday  = 0;

    if (ruleMonth >= 1 && ruleMonth <= 12) {
        // M.m.w stored directly in (month,week,day)
        int dom = findDayOfMonth(year, ruleMonth, ruleDay, ruleWeek);
        month = ruleMonth;
        mday  = dom;
    } else if (ruleMonth == 13) {
        // Jn : 1..365 ignoring Feb 29
        int n = (int)getPackedDoy(ruleWeek, ruleDay); // 1..365
        if (n < 1 || n > 365) return 0;

        // Convert Jn to 0-based day-of-year that *includes* Feb 29 for leap years
        // POSIX Jn ignores Feb 29, so for leap years, days >= 60 shift by +1.
        int doy0 = n - 1; // 0-based, ignoring Feb29
        if (is_leap(year) && n >= 60) {
            doy0 += 1;
        }

        if (!doy_to_month_day(year, doy0, &month, &mday)) return 0;
    } else if (ruleMonth == 0) {
        // n : 0..365 including Feb 29
        int n = (int)getPackedDoy(ruleWeek, ruleDay); // 0..365
        int maxN = is_leap(year) ? 365 : 364;
        if (n < 0 || n > maxN) return 0;

        if (!doy_to_month_day(year, n, &month, &mday)) return 0;
    } else {
        return 0;
    }

    struct tm localTm;
    memset(&localTm, 0, sizeof(localTm));
    localTm.tm_year = year - 1900;
    localTm.tm_mon  = month - 1;
    localTm.tm_mday = mday;
    localTm.tm_hour = ruleHour;
    localTm.tm_min  = 0;
    localTm.tm_sec  = 0;

    // Convert "local transition time" to UTC using the offset relevant at that boundary.
    time_t localAsUtcEpoch = mktime_utc_local(&localTm);
    if (localAsUtcEpoch == (time_t)-1) return 0;

    *outUtc = localAsUtcEpoch - (time_t)offsetUsedToConvertLocalToUtc;
    return 1;
}

bool isDstActive(const TimezoneInfo* tz, time_t utcTime) {
    if (!tz || !tz->hasDST) return false;

    struct tm* utc = gmtime(&utcTime);
    if (!utc) return false;

    int year = utc->tm_year + 1900;

    // If DST rules are absent, policy remains: assume DST always active.
    // (If you prefer "DST never active" instead, return false here.)
    if (tz->dstStartMonth == 0 && tz->dstStartWeek == 0 && tz->dstStartDay == 0 &&
        tz->dstEndMonth   == 0 && tz->dstEndWeek   == 0 && tz->dstEndDay   == 0) {
        return true;
    }

    time_t dstStartUtc = 0;
    time_t dstEndUtc   = 0;

    // Start boundary uses stdOffset to convert local->UTC
    if (!buildTransitionUtc(tz, year,
                            tz->dstStartMonth, tz->dstStartWeek, tz->dstStartDay, tz->dstStartHour,
                            tz->stdOffset,
                            &dstStartUtc)) {
        return false;
    }

    // End boundary uses dstOffset to convert local->UTC
    if (!buildTransitionUtc(tz, year,
                            tz->dstEndMonth, tz->dstEndWeek, tz->dstEndDay, tz->dstEndHour,
                            tz->dstOffset,
                            &dstEndUtc)) {
        return false;
    }

    if (dstStartUtc < dstEndUtc) {
        return (utcTime >= dstStartUtc && utcTime < dstEndUtc);
    } else {
        return (utcTime >= dstStartUtc || utcTime < dstEndUtc);
    }
}

long getCurrentOffset(const TimezoneInfo* tz, time_t utcTime) {
    if (!tz) return 0;
    return isDstActive(tz, utcTime) ? tz->dstOffset : tz->stdOffset;
}

char* getCityLocalTime(const char* tzString, char* timeTextBuffer) {
    uint8_t pre_Hr[MAX_WORLD_CLOCKS] = {111, 111, 111, 111, 111};
    uint8_t pre_Min[MAX_WORLD_CLOCKS] = {111, 111, 111, 111, 111};

    if (!tzString || !timeTextBuffer) return timeTextBuffer;

    // Per header contract: must be previously cached
    int tzIndex = findTimezone(tzString);
    if (tzIndex < 0) {
        timeTextBuffer[0] = '\0';
        printf("Program Returned 1st\n");
        return timeTextBuffer;
    }

    time_t utcTime;
    time(&utcTime);

    long offset = getCurrentOffset(&tzCache[tzIndex], utcTime);
    time_t localTime = utcTime + (time_t)offset;

    struct tm* now = gmtime(&localTime);
    if (!now) {
        timeTextBuffer[0] = '\0';
        printf("Program Returned 2nd\n");
        return timeTextBuffer;
    }

    uint8_t hr  = (uint8_t)now->tm_hour;
    uint8_t min = (uint8_t)now->tm_min;

    // Always output HH:MM
    pre_Hr[tzIndex] = hr;
    pre_Min[tzIndex] = min;

    timeTextBuffer[0] = (char)('0' + (hr / 10));
    timeTextBuffer[1] = (char)('0' + (hr % 10));
    timeTextBuffer[2] = ':';
    timeTextBuffer[3] = (char)('0' + (min / 10));
    timeTextBuffer[4] = (char)('0' + (min % 10));
    timeTextBuffer[5] = '\0';
    
    return timeTextBuffer;
}

bool getLocalTm(const char* tzString, struct tm* tmOut) {
    if (!tzString || !tmOut) return false;

    int tzIndex = findTimezone(tzString);
    if (tzIndex < 0) return false;

    time_t utcTime;
    time(&utcTime);

    long offset = getCurrentOffset(&tzCache[tzIndex], utcTime);
    time_t localTime = utcTime + (time_t)offset;

    struct tm* result = gmtime(&localTime);
    if (!result) return false;

    memcpy(tmOut, result, sizeof(struct tm));
    return true;
}
