#ifndef MCUJS_ESP32_USB_MSC_H
#define MCUJS_ESP32_USB_MSC_H

#include <stdbool.h>

/* Retains the TinyUSB callbacks before driver installation. */
void mcujs_usb_msc_init(void);

/* Main-task-only ownership transitions. */
bool mcujs_usb_msc_expose(void);
void mcujs_usb_msc_task(void);

#endif /* MCUJS_ESP32_USB_MSC_H */
