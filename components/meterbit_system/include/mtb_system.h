#ifndef __POWER_H__
#define __POWER_H__

#include "mtb_text_scroll.h"

    typedef enum {TEMP_SH = 1, PERM_SH, TEMP_RS, STAY_ON} power_States_t;
    extern power_States_t wake;
    extern void system_Init(void);
    extern void device_SD_RS(power_States_t);
    extern void rotaryEncoder_Init(void);

#endif  //__POWER_H__