/*
 * mcujs - USB MSC Interface
 * 
 * Mass Storage Class for exposing filesystem
 */

#ifndef MCUJS_USB_MSC_H
#define MCUJS_USB_MSC_H

#include <stdint.h>
#include <stdbool.h>

#include "msc_ownership.h"

/*
 * Initialize USB MSC
 * Must be called after filesystem is initialized
 */
void usb_msc_init(void);

/* Main-loop-only ownership transitions. */
bool usb_msc_expose(void);
void usb_msc_task(void);
void usb_msc_event(mcujs_msc_event_t event);

/*
 * Check if the drive has been ejected
 */
bool usb_msc_ejected(void);

/*
 * Reset ejected state (after re-insertion)
 */
void usb_msc_reset_ejected(void);

/*
 * Signal to host that media has changed
 * This causes the host to re-read the filesystem
 */
void usb_msc_media_changed(void);

#endif /* MCUJS_USB_MSC_H */
