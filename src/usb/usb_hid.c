/*
 * mcujs - USB HID (Human Interface Device)
 * 
 * Keyboard and mouse emulation using TinyUSB HID class.
 * Provides low-level HID report generation and USB callbacks.
 */

#include "tusb.h"
#include "usb_hid.h"

#if CFG_TUD_HID

/* Report IDs must match descriptor */
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_CONSUMER 3

/* Current keyboard state */
static uint8_t keyboard_modifier = 0;
static uint8_t keyboard_keycodes[6] = {0};
static uint8_t keyboard_keycode_count = 0;

/* Current mouse button state */
static uint8_t mouse_buttons = 0;

/*--------------------------------------------------------------------
 * TinyUSB HID Callbacks
 *--------------------------------------------------------------------*/

/* Invoked when received GET_REPORT control request */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_type;
    (void)reqlen;
    
    if (report_id == REPORT_ID_KEYBOARD) {
        /* Return current keyboard state */
        buffer[0] = keyboard_modifier;
        buffer[1] = 0; /* Reserved */
        for (int i = 0; i < 6; i++) {
            buffer[2 + i] = keyboard_keycodes[i];
        }
        return 8;
    } else if (report_id == REPORT_ID_MOUSE) {
        /* Return current mouse state (no movement) */
        buffer[0] = mouse_buttons;
        buffer[1] = 0; /* X */
        buffer[2] = 0; /* Y */
        buffer[3] = 0; /* Wheel */
        return 4;
    }
    
    return 0;
}

/* Invoked when received SET_REPORT control request or data on OUT endpoint */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
    
    /* Handle LED status from host (Caps Lock, Num Lock, etc.) */
    /* For now, we ignore these */
}

/*--------------------------------------------------------------------
 * Helper Functions
 *--------------------------------------------------------------------*/

/* Wait for HID to be ready, with timeout (in iterations) */
static bool wait_hid_ready(uint32_t timeout) {
    while (!tud_hid_ready() && timeout > 0) {
        tud_task();  /* Process USB events */
        timeout--;
    }
    return tud_hid_ready();
}

/* Send keyboard report and wait for it to complete */
static bool send_keyboard_report(void) {
    if (!wait_hid_ready(10000)) {
        return false;
    }
    bool result = tud_hid_keyboard_report(REPORT_ID_KEYBOARD, keyboard_modifier, keyboard_keycodes);
    /* Give USB stack time to send the report */
    for (int i = 0; i < 1000; i++) {
        tud_task();
    }
    return result;
}

/*--------------------------------------------------------------------
 * Keyboard Functions
 *--------------------------------------------------------------------*/

bool usb_hid_keyboard_ready(void) {
    return tud_hid_ready();
}

void usb_hid_keyboard_release_all(void) {
    keyboard_modifier = 0;
    keyboard_keycode_count = 0;
    for (int i = 0; i < 6; i++) {
        keyboard_keycodes[i] = 0;
    }
    
    send_keyboard_report();
}

bool usb_hid_keyboard_press(uint8_t keycode) {
    /* Check if it's a modifier key */
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
        uint8_t modifier_bit = 1 << (keycode - HID_KEY_CONTROL_LEFT);
        keyboard_modifier |= modifier_bit;
    } else {
        /* Regular key - add to keycodes array if not already present */
        bool found = false;
        for (int i = 0; i < keyboard_keycode_count; i++) {
            if (keyboard_keycodes[i] == keycode) {
                found = true;
                break;
            }
        }
        if (!found && keyboard_keycode_count < 6) {
            keyboard_keycodes[keyboard_keycode_count++] = keycode;
        }
    }
    
    return send_keyboard_report();
}

bool usb_hid_keyboard_release(uint8_t keycode) {
    /* Check if it's a modifier key */
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
        uint8_t modifier_bit = 1 << (keycode - HID_KEY_CONTROL_LEFT);
        keyboard_modifier &= ~modifier_bit;
    } else {
        /* Regular key - remove from keycodes array */
        for (int i = 0; i < keyboard_keycode_count; i++) {
            if (keyboard_keycodes[i] == keycode) {
                /* Shift remaining keys down */
                for (int j = i; j < keyboard_keycode_count - 1; j++) {
                    keyboard_keycodes[j] = keyboard_keycodes[j + 1];
                }
                keyboard_keycodes[keyboard_keycode_count - 1] = 0;
                keyboard_keycode_count--;
                break;
            }
        }
    }
    
    return send_keyboard_report();
}

bool usb_hid_keyboard_is_pressed(uint8_t keycode) {
    if (keycode >= HID_KEY_CONTROL_LEFT && keycode <= HID_KEY_GUI_RIGHT) {
        uint8_t modifier_bit = 1 << (keycode - HID_KEY_CONTROL_LEFT);
        return (keyboard_modifier & modifier_bit) != 0;
    } else {
        for (int i = 0; i < keyboard_keycode_count; i++) {
            if (keyboard_keycodes[i] == keycode) {
                return true;
            }
        }
    }
    return false;
}

/*--------------------------------------------------------------------
 * Mouse Functions
 *--------------------------------------------------------------------*/

/* Send mouse report and wait for it to complete */
static bool send_mouse_report(int8_t x, int8_t y, int8_t wheel, int8_t pan) {
    if (!wait_hid_ready(10000)) {
        return false;
    }
    bool result = tud_hid_mouse_report(REPORT_ID_MOUSE, mouse_buttons, x, y, wheel, pan);
    /* Give USB stack time to send the report */
    for (int i = 0; i < 1000; i++) {
        tud_task();
    }
    return result;
}

bool usb_hid_mouse_ready(void) {
    return tud_hid_ready();
}

void usb_hid_mouse_release_all(void) {
    mouse_buttons = 0;
    send_mouse_report(0, 0, 0, 0);
}

bool usb_hid_mouse_move(int8_t x, int8_t y) {
    return send_mouse_report(x, y, 0, 0);
}

bool usb_hid_mouse_scroll(int8_t vertical, int8_t horizontal) {
    return send_mouse_report(0, 0, vertical, horizontal);
}

bool usb_hid_mouse_press(uint8_t button) {
    mouse_buttons |= button;
    return send_mouse_report(0, 0, 0, 0);
}

bool usb_hid_mouse_release(uint8_t button) {
    mouse_buttons &= ~button;
    return send_mouse_report(0, 0, 0, 0);
}

bool usb_hid_mouse_is_pressed(uint8_t button) {
    return (mouse_buttons & button) != 0;
}

/*--------------------------------------------------------------------
 * Consumer Control (Media Keys) Functions
 *--------------------------------------------------------------------*/

/* Send a consumer control report */
static bool send_consumer_report(uint16_t usage_code) {
    if (!wait_hid_ready(10000)) {
        return false;
    }
    bool result = tud_hid_report(REPORT_ID_CONSUMER, &usage_code, sizeof(usage_code));
    /* Give USB stack time to send the report */
    for (int i = 0; i < 1000; i++) {
        tud_task();
    }
    return result;
}

bool usb_hid_consumer_press(uint16_t usage_code) {
    return send_consumer_report(usage_code);
}

bool usb_hid_consumer_release(void) {
    return send_consumer_report(0);
}

bool usb_hid_consumer_tap(uint16_t usage_code) {
    bool result = usb_hid_consumer_press(usage_code);
    if (result) {
        /* Small delay for host to register */
        for (int i = 0; i < 500; i++) {
            tud_task();
        }
        result = usb_hid_consumer_release();
    }
    return result;
}

#endif /* CFG_TUD_HID */
