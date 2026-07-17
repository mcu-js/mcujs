/*
 * mcujs - Mouse Module
 * 
 * JavaScript bindings for USB HID mouse emulation.
 * 
 * API:
 *   Mouse.move(x, y)        - Move mouse relative to current position
 *   Mouse.click(button)     - Click a button (press and release)
 *   Mouse.press(button)     - Press a button
 *   Mouse.release(button)   - Release a button
 *   Mouse.doubleClick(btn)  - Double-click a button
 *   Mouse.scroll(amount)    - Scroll vertically
 *   Mouse.scrollH(amount)   - Scroll horizontally
 *   Mouse.releaseAll()      - Release all buttons
 *   Mouse.isPressed(button) - Check if button is pressed
 */

#include "bindings.h"
#include "jerryscript.h"

#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"  /* For sleep_ms */

/* Check if HID is enabled via tusb_config.h */
#include "tusb_config.h"

#if CFG_TUD_HID

#include "usb_hid.h"

/*--------------------------------------------------------------------
 * Button Name Mapping
 *--------------------------------------------------------------------*/

static uint8_t lookup_button(const char *name) {
    /* Convert to lowercase for comparison */
    char lower[16];
    size_t len = strlen(name);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)name[i]);
    }
    lower[len] = '\0';
    
    if (strcmp(lower, "left") == 0 || strcmp(lower, "l") == 0) {
        return USB_HID_MOUSE_BUTTON_LEFT;
    } else if (strcmp(lower, "right") == 0 || strcmp(lower, "r") == 0) {
        return USB_HID_MOUSE_BUTTON_RIGHT;
    } else if (strcmp(lower, "middle") == 0 || strcmp(lower, "m") == 0) {
        return USB_HID_MOUSE_BUTTON_MIDDLE;
    }
    
    return 0;  /* Unknown */
}

/*--------------------------------------------------------------------
 * JavaScript Handlers
 *--------------------------------------------------------------------*/

/* Mouse.move(x, y) */
static jerry_value_t mouse_move_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "move requires x and y");
    }
    
    if (!jerry_value_is_number(args[0]) || !jerry_value_is_number(args[1])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "x and y must be numbers");
    }
    
    int32_t x = (int32_t)jerry_value_as_number(args[0]);
    int32_t y = (int32_t)jerry_value_as_number(args[1]);
    
    /* Clamp to int8_t range */
    if (x < -127) x = -127;
    if (x > 127) x = 127;
    if (y < -127) y = -127;
    if (y > 127) y = 127;
    
    usb_hid_mouse_move((int8_t)x, (int8_t)y);
    return jerry_undefined();
}

/* Mouse.press(button) */
static jerry_value_t mouse_press_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "button must be a string");
    }
    
    char btn_name[16];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)btn_name,
                                               sizeof(btn_name) - 1);
    btn_name[len] = '\0';
    
    uint8_t button = lookup_button(btn_name);
    if (button == 0) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown button (use left, right, middle)");
    }
    
    usb_hid_mouse_press(button);
    return jerry_undefined();
}

/* Mouse.release(button) */
static jerry_value_t mouse_release_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "button must be a string");
    }
    
    char btn_name[16];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)btn_name,
                                               sizeof(btn_name) - 1);
    btn_name[len] = '\0';
    
    uint8_t button = lookup_button(btn_name);
    if (button == 0) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown button (use left, right, middle)");
    }
    
    usb_hid_mouse_release(button);
    return jerry_undefined();
}

/* Mouse.click(button) - press and release */
static jerry_value_t mouse_click_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    /* Default to left button */
    uint8_t button = USB_HID_MOUSE_BUTTON_LEFT;
    
    if (argc >= 1 && jerry_value_is_string(args[0])) {
        char btn_name[16];
        jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                                   (jerry_char_t *)btn_name,
                                                   sizeof(btn_name) - 1);
        btn_name[len] = '\0';
        
        button = lookup_button(btn_name);
        if (button == 0) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown button (use left, right, middle)");
        }
    }
    
    usb_hid_mouse_press(button);
    sleep_ms(10);
    usb_hid_mouse_release(button);
    
    return jerry_undefined();
}

/* Mouse.doubleClick(button) */
static jerry_value_t mouse_double_click_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    /* Default to left button */
    uint8_t button = USB_HID_MOUSE_BUTTON_LEFT;
    
    if (argc >= 1 && jerry_value_is_string(args[0])) {
        char btn_name[16];
        jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                                   (jerry_char_t *)btn_name,
                                                   sizeof(btn_name) - 1);
        btn_name[len] = '\0';
        
        button = lookup_button(btn_name);
        if (button == 0) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown button (use left, right, middle)");
        }
    }
    
    /* First click */
    usb_hid_mouse_press(button);
    sleep_ms(10);
    usb_hid_mouse_release(button);
    
    /* Short delay between clicks */
    sleep_ms(50);
    
    /* Second click */
    usb_hid_mouse_press(button);
    sleep_ms(10);
    usb_hid_mouse_release(button);
    
    return jerry_undefined();
}

/* Mouse.scroll(amount) - vertical scroll */
static jerry_value_t mouse_scroll_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "amount must be a number");
    }
    
    int32_t amount = (int32_t)jerry_value_as_number(args[0]);
    
    /* Clamp to int8_t range */
    if (amount < -127) amount = -127;
    if (amount > 127) amount = 127;
    
    usb_hid_mouse_scroll((int8_t)amount, 0);
    return jerry_undefined();
}

/* Mouse.scrollH(amount) - horizontal scroll */
static jerry_value_t mouse_scroll_h_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_number(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "amount must be a number");
    }
    
    int32_t amount = (int32_t)jerry_value_as_number(args[0]);
    
    /* Clamp to int8_t range */
    if (amount < -127) amount = -127;
    if (amount > 127) amount = 127;
    
    usb_hid_mouse_scroll(0, (int8_t)amount);
    return jerry_undefined();
}

/* Mouse.releaseAll() */
static jerry_value_t mouse_release_all_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    usb_hid_mouse_release_all();
    return jerry_undefined();
}

/* Mouse.isPressed(button) */
static jerry_value_t mouse_is_pressed_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "button must be a string");
    }
    
    char btn_name[16];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)btn_name,
                                               sizeof(btn_name) - 1);
    btn_name[len] = '\0';
    
    uint8_t button = lookup_button(btn_name);
    if (button == 0) {
        return jerry_boolean(false);
    }
    
    return jerry_boolean(usb_hid_mouse_is_pressed(button));
}

/*--------------------------------------------------------------------
 * Module Creation
 *--------------------------------------------------------------------*/

jerry_value_t js_create_mouse_module(void) {
    jerry_value_t module = jerry_object();
    
    js_set_function(module, "move", mouse_move_handler);
    js_set_function(module, "press", mouse_press_handler);
    js_set_function(module, "release", mouse_release_handler);
    js_set_function(module, "click", mouse_click_handler);
    js_set_function(module, "doubleClick", mouse_double_click_handler);
    js_set_function(module, "scroll", mouse_scroll_handler);
    js_set_function(module, "scrollH", mouse_scroll_h_handler);
    js_set_function(module, "releaseAll", mouse_release_all_handler);
    js_set_function(module, "isPressed", mouse_is_pressed_handler);
    
    return module;
}

#else /* CFG_TUD_HID */

/* Stub when HID is disabled */
jerry_value_t js_create_mouse_module(void) {
    return jerry_throw_sz(JERRY_ERROR_COMMON, "HID not enabled");
}

#endif /* CFG_TUD_HID */
