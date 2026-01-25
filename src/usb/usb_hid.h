/*
 * mcujs - USB HID Header
 * 
 * Low-level HID functions for keyboard and mouse emulation.
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declare - tusb_config.h defines CFG_TUD_HID */
#include "tusb_config.h"

#if CFG_TUD_HID

/* Mouse button definitions (from TinyUSB hid.h) */
#define USB_HID_MOUSE_BUTTON_LEFT   0x01
#define USB_HID_MOUSE_BUTTON_RIGHT  0x02
#define USB_HID_MOUSE_BUTTON_MIDDLE 0x04

/*--------------------------------------------------------------------
 * Keyboard Functions
 *--------------------------------------------------------------------*/

/* Check if HID is ready to send keyboard reports */
bool usb_hid_keyboard_ready(void);

/* Release all keys */
void usb_hid_keyboard_release_all(void);

/* Press a key (keycode from hid.h, e.g., HID_KEY_A) */
bool usb_hid_keyboard_press(uint8_t keycode);

/* Release a key */
bool usb_hid_keyboard_release(uint8_t keycode);

/* Check if a key is currently pressed */
bool usb_hid_keyboard_is_pressed(uint8_t keycode);

/*--------------------------------------------------------------------
 * Mouse Functions
 *--------------------------------------------------------------------*/

/* Check if HID is ready to send mouse reports */
bool usb_hid_mouse_ready(void);

/* Release all mouse buttons */
void usb_hid_mouse_release_all(void);

/* Move mouse relative to current position */
bool usb_hid_mouse_move(int8_t x, int8_t y);

/* Scroll (vertical and horizontal) */
bool usb_hid_mouse_scroll(int8_t vertical, int8_t horizontal);

/* Press a mouse button (MOUSE_BUTTON_LEFT, etc.) */
bool usb_hid_mouse_press(uint8_t button);

/* Release a mouse button */
bool usb_hid_mouse_release(uint8_t button);

/* Check if a mouse button is pressed */
bool usb_hid_mouse_is_pressed(uint8_t button);

#endif /* CFG_TUD_HID */

#endif /* USB_HID_H */
