#ifndef MCUJS_ESP32_USB_RECOVERY_H
#define MCUJS_ESP32_USB_RECOVERY_H

/* Arm before TinyUSB initialization; both functions run on the main task. */
void mcujs_usb_recovery_start(void);
void mcujs_usb_recovery_mark_healthy(void);

#endif /* MCUJS_ESP32_USB_RECOVERY_H */
