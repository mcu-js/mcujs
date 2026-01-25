/*
 * mcujs - Keyboard Module
 * 
 * JavaScript bindings for USB HID keyboard emulation.
 * 
 * API:
 *   Keyboard.press(key)     - Press a key
 *   Keyboard.release(key)   - Release a key
 *   Keyboard.tap(key)       - Press and release a key
 *   Keyboard.print(string)  - Type a string
 *   Keyboard.releaseAll()   - Release all keys
 *   Keyboard.isPressed(key) - Check if key is pressed
 */

#include "bindings.h"
#include "jerryscript.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "pico/stdlib.h"  /* For sleep_ms */

/* Check if HID is enabled via tusb_config.h */
#include "tusb_config.h"

#if CFG_TUD_HID

#include "usb_hid.h"
#include "class/hid/hid.h"  /* For HID_KEY_* constants */

/*--------------------------------------------------------------------
 * Key Name to HID Keycode Mapping
 *--------------------------------------------------------------------*/

typedef struct {
    const char *name;
    uint8_t keycode;
    bool needs_shift;
} key_mapping_t;

/* Map key names to HID keycodes */
static const key_mapping_t key_map[] = {
    /* Letters */
    {"a", HID_KEY_A, false}, {"b", HID_KEY_B, false}, {"c", HID_KEY_C, false},
    {"d", HID_KEY_D, false}, {"e", HID_KEY_E, false}, {"f", HID_KEY_F, false},
    {"g", HID_KEY_G, false}, {"h", HID_KEY_H, false}, {"i", HID_KEY_I, false},
    {"j", HID_KEY_J, false}, {"k", HID_KEY_K, false}, {"l", HID_KEY_L, false},
    {"m", HID_KEY_M, false}, {"n", HID_KEY_N, false}, {"o", HID_KEY_O, false},
    {"p", HID_KEY_P, false}, {"q", HID_KEY_Q, false}, {"r", HID_KEY_R, false},
    {"s", HID_KEY_S, false}, {"t", HID_KEY_T, false}, {"u", HID_KEY_U, false},
    {"v", HID_KEY_V, false}, {"w", HID_KEY_W, false}, {"x", HID_KEY_X, false},
    {"y", HID_KEY_Y, false}, {"z", HID_KEY_Z, false},
    
    /* Numbers */
    {"0", HID_KEY_0, false}, {"1", HID_KEY_1, false}, {"2", HID_KEY_2, false},
    {"3", HID_KEY_3, false}, {"4", HID_KEY_4, false}, {"5", HID_KEY_5, false},
    {"6", HID_KEY_6, false}, {"7", HID_KEY_7, false}, {"8", HID_KEY_8, false},
    {"9", HID_KEY_9, false},
    
    /* Function keys */
    {"f1", HID_KEY_F1, false}, {"f2", HID_KEY_F2, false}, {"f3", HID_KEY_F3, false},
    {"f4", HID_KEY_F4, false}, {"f5", HID_KEY_F5, false}, {"f6", HID_KEY_F6, false},
    {"f7", HID_KEY_F7, false}, {"f8", HID_KEY_F8, false}, {"f9", HID_KEY_F9, false},
    {"f10", HID_KEY_F10, false}, {"f11", HID_KEY_F11, false}, {"f12", HID_KEY_F12, false},
    
    /* Modifiers */
    {"ctrl", HID_KEY_CONTROL_LEFT, false},
    {"control", HID_KEY_CONTROL_LEFT, false},
    {"shift", HID_KEY_SHIFT_LEFT, false},
    {"alt", HID_KEY_ALT_LEFT, false},
    {"option", HID_KEY_ALT_LEFT, false},
    {"gui", HID_KEY_GUI_LEFT, false},
    {"super", HID_KEY_GUI_LEFT, false},
    {"cmd", HID_KEY_GUI_LEFT, false},
    {"command", HID_KEY_GUI_LEFT, false},
    {"win", HID_KEY_GUI_LEFT, false},
    {"windows", HID_KEY_GUI_LEFT, false},
    {"meta", HID_KEY_GUI_LEFT, false},
    
    /* Right-side modifiers */
    {"rctrl", HID_KEY_CONTROL_RIGHT, false},
    {"rshift", HID_KEY_SHIFT_RIGHT, false},
    {"ralt", HID_KEY_ALT_RIGHT, false},
    {"rgui", HID_KEY_GUI_RIGHT, false},
    
    /* Special keys */
    {"enter", HID_KEY_ENTER, false},
    {"return", HID_KEY_ENTER, false},
    {"tab", HID_KEY_TAB, false},
    {"space", HID_KEY_SPACE, false},
    {"backspace", HID_KEY_BACKSPACE, false},
    {"delete", HID_KEY_DELETE, false},
    {"escape", HID_KEY_ESCAPE, false},
    {"esc", HID_KEY_ESCAPE, false},
    
    /* Navigation */
    {"up", HID_KEY_ARROW_UP, false},
    {"down", HID_KEY_ARROW_DOWN, false},
    {"left", HID_KEY_ARROW_LEFT, false},
    {"right", HID_KEY_ARROW_RIGHT, false},
    {"home", HID_KEY_HOME, false},
    {"end", HID_KEY_END, false},
    {"pageup", HID_KEY_PAGE_UP, false},
    {"pagedown", HID_KEY_PAGE_DOWN, false},
    {"insert", HID_KEY_INSERT, false},
    
    /* Locks */
    {"capslock", HID_KEY_CAPS_LOCK, false},
    {"numlock", HID_KEY_NUM_LOCK, false},
    {"scrolllock", HID_KEY_SCROLL_LOCK, false},
    
    /* Misc */
    {"printscreen", HID_KEY_PRINT_SCREEN, false},
    {"pause", HID_KEY_PAUSE, false},
    
    /* Punctuation (unshifted) */
    {"-", HID_KEY_MINUS, false},
    {"=", HID_KEY_EQUAL, false},
    {"[", HID_KEY_BRACKET_LEFT, false},
    {"]", HID_KEY_BRACKET_RIGHT, false},
    {"\\", HID_KEY_BACKSLASH, false},
    {";", HID_KEY_SEMICOLON, false},
    {"'", HID_KEY_APOSTROPHE, false},
    {"`", HID_KEY_GRAVE, false},
    {",", HID_KEY_COMMA, false},
    {".", HID_KEY_PERIOD, false},
    {"/", HID_KEY_SLASH, false},
    
    {NULL, 0, false}  /* End marker */
};

/* Convert character to keycode (for print function) */
static bool char_to_keycode(char c, uint8_t *keycode, bool *needs_shift) {
    *needs_shift = false;
    
    /* Lowercase letters */
    if (c >= 'a' && c <= 'z') {
        *keycode = HID_KEY_A + (c - 'a');
        return true;
    }
    
    /* Uppercase letters */
    if (c >= 'A' && c <= 'Z') {
        *keycode = HID_KEY_A + (c - 'A');
        *needs_shift = true;
        return true;
    }
    
    /* Numbers */
    if (c >= '1' && c <= '9') {
        *keycode = HID_KEY_1 + (c - '1');
        return true;
    }
    if (c == '0') {
        *keycode = HID_KEY_0;
        return true;
    }
    
    /* Shifted number row symbols */
    switch (c) {
        case '!': *keycode = HID_KEY_1; *needs_shift = true; return true;
        case '@': *keycode = HID_KEY_2; *needs_shift = true; return true;
        case '#': *keycode = HID_KEY_3; *needs_shift = true; return true;
        case '$': *keycode = HID_KEY_4; *needs_shift = true; return true;
        case '%': *keycode = HID_KEY_5; *needs_shift = true; return true;
        case '^': *keycode = HID_KEY_6; *needs_shift = true; return true;
        case '&': *keycode = HID_KEY_7; *needs_shift = true; return true;
        case '*': *keycode = HID_KEY_8; *needs_shift = true; return true;
        case '(': *keycode = HID_KEY_9; *needs_shift = true; return true;
        case ')': *keycode = HID_KEY_0; *needs_shift = true; return true;
    }
    
    /* Other characters */
    switch (c) {
        case ' ': *keycode = HID_KEY_SPACE; return true;
        case '\n': *keycode = HID_KEY_ENTER; return true;
        case '\t': *keycode = HID_KEY_TAB; return true;
        case '-': *keycode = HID_KEY_MINUS; return true;
        case '_': *keycode = HID_KEY_MINUS; *needs_shift = true; return true;
        case '=': *keycode = HID_KEY_EQUAL; return true;
        case '+': *keycode = HID_KEY_EQUAL; *needs_shift = true; return true;
        case '[': *keycode = HID_KEY_BRACKET_LEFT; return true;
        case '{': *keycode = HID_KEY_BRACKET_LEFT; *needs_shift = true; return true;
        case ']': *keycode = HID_KEY_BRACKET_RIGHT; return true;
        case '}': *keycode = HID_KEY_BRACKET_RIGHT; *needs_shift = true; return true;
        case '\\': *keycode = HID_KEY_BACKSLASH; return true;
        case '|': *keycode = HID_KEY_BACKSLASH; *needs_shift = true; return true;
        case ';': *keycode = HID_KEY_SEMICOLON; return true;
        case ':': *keycode = HID_KEY_SEMICOLON; *needs_shift = true; return true;
        case '\'': *keycode = HID_KEY_APOSTROPHE; return true;
        case '"': *keycode = HID_KEY_APOSTROPHE; *needs_shift = true; return true;
        case '`': *keycode = HID_KEY_GRAVE; return true;
        case '~': *keycode = HID_KEY_GRAVE; *needs_shift = true; return true;
        case ',': *keycode = HID_KEY_COMMA; return true;
        case '<': *keycode = HID_KEY_COMMA; *needs_shift = true; return true;
        case '.': *keycode = HID_KEY_PERIOD; return true;
        case '>': *keycode = HID_KEY_PERIOD; *needs_shift = true; return true;
        case '/': *keycode = HID_KEY_SLASH; return true;
        case '?': *keycode = HID_KEY_SLASH; *needs_shift = true; return true;
    }
    
    return false;
}

/* Look up key name in table */
static bool lookup_key(const char *name, uint8_t *keycode) {
    /* Convert to lowercase for comparison */
    char lower[32];
    size_t len = strlen(name);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)name[i]);
    }
    lower[len] = '\0';
    
    /* Search table */
    for (const key_mapping_t *m = key_map; m->name != NULL; m++) {
        if (strcmp(lower, m->name) == 0) {
            *keycode = m->keycode;
            return true;
        }
    }
    
    /* Single character? */
    if (len == 1) {
        bool needs_shift;
        return char_to_keycode(name[0], keycode, &needs_shift);
    }
    
    return false;
}

/*--------------------------------------------------------------------
 * JavaScript Handlers
 *--------------------------------------------------------------------*/

/* Keyboard.press(key) */
static jerry_value_t keyboard_press_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "key must be a string");
    }
    
    char key_name[32];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)key_name,
                                               sizeof(key_name) - 1);
    key_name[len] = '\0';
    
    uint8_t keycode;
    if (!lookup_key(key_name, &keycode)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown key");
    }
    
    usb_hid_keyboard_press(keycode);
    return jerry_undefined();
}

/* Keyboard.release(key) */
static jerry_value_t keyboard_release_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "key must be a string");
    }
    
    char key_name[32];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)key_name,
                                               sizeof(key_name) - 1);
    key_name[len] = '\0';
    
    uint8_t keycode;
    if (!lookup_key(key_name, &keycode)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown key");
    }
    
    usb_hid_keyboard_release(keycode);
    return jerry_undefined();
}

/* Keyboard.tap(key) - press and release */
static jerry_value_t keyboard_tap_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "key must be a string");
    }
    
    char key_name[32];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)key_name,
                                               sizeof(key_name) - 1);
    key_name[len] = '\0';
    
    uint8_t keycode;
    if (!lookup_key(key_name, &keycode)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unknown key");
    }
    
    usb_hid_keyboard_press(keycode);
    sleep_ms(10);  /* Hold key for 10ms */
    usb_hid_keyboard_release(keycode);
    
    return jerry_undefined();
}

/* Keyboard.print(string) - type a string */
static jerry_value_t keyboard_print_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "text must be a string");
    }
    
    jerry_size_t len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    if (len > 1024) len = 1024;  /* Limit string length */
    
    char *text = malloc(len + 1);
    if (!text) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory");
    }
    
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, (jerry_char_t *)text, len);
    text[len] = '\0';
    
    /* Type each character */
    for (size_t i = 0; i < len; i++) {
        uint8_t keycode;
        bool needs_shift;
        
        if (char_to_keycode(text[i], &keycode, &needs_shift)) {
            if (needs_shift) {
                usb_hid_keyboard_press(HID_KEY_SHIFT_LEFT);
                sleep_ms(5);  /* Let shift register */
            }
            usb_hid_keyboard_press(keycode);
            sleep_ms(10);  /* Hold key for 10ms */
            usb_hid_keyboard_release(keycode);
            if (needs_shift) {
                usb_hid_keyboard_release(HID_KEY_SHIFT_LEFT);
            }
            sleep_ms(5);  /* Inter-character delay */
        }
    }
    
    free(text);
    return jerry_undefined();
}

/* Keyboard.releaseAll() */
static jerry_value_t keyboard_release_all_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    usb_hid_keyboard_release_all();
    return jerry_undefined();
}

/* Keyboard.isPressed(key) */
static jerry_value_t keyboard_is_pressed_handler(
    const jerry_call_info_t *call_info_p,
    const jerry_value_t args[],
    const jerry_length_t argc)
{
    (void)call_info_p;
    
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "key must be a string");
    }
    
    char key_name[32];
    jerry_size_t len = jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                                               (jerry_char_t *)key_name,
                                               sizeof(key_name) - 1);
    key_name[len] = '\0';
    
    uint8_t keycode;
    if (!lookup_key(key_name, &keycode)) {
        return jerry_boolean(false);
    }
    
    return jerry_boolean(usb_hid_keyboard_is_pressed(keycode));
}

/*--------------------------------------------------------------------
 * Module Creation
 *--------------------------------------------------------------------*/

jerry_value_t js_create_keyboard_module(void) {
    jerry_value_t module = jerry_object();
    
    js_set_function(module, "press", keyboard_press_handler);
    js_set_function(module, "release", keyboard_release_handler);
    js_set_function(module, "tap", keyboard_tap_handler);
    js_set_function(module, "print", keyboard_print_handler);
    js_set_function(module, "releaseAll", keyboard_release_all_handler);
    js_set_function(module, "isPressed", keyboard_is_pressed_handler);
    
    return module;
}

#else /* CFG_TUD_HID */

/* Stub when HID is disabled */
jerry_value_t js_create_keyboard_module(void) {
    return jerry_throw_sz(JERRY_ERROR_COMMON, "HID not enabled");
}

#endif /* CFG_TUD_HID */
