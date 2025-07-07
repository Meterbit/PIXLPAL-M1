
#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void file_operations(void);
extern TaskHandle_t usb_Mass_Storage_H;

#ifdef __cplusplus
}
#endif

#endif