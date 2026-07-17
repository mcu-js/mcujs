/* Minimal filesystem-free REPL for ESP32-S3 bring-up. */

#include "repl.h"
#include "engine.h"
#include "usb_cdc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define REPL_LINE_SIZE 256
#define REPL_RESULT_SIZE 512

static char s_line[REPL_LINE_SIZE];
static size_t s_length;
static bool s_initialized;
static bool s_prompt_shown;
static bool s_ignore_lf;

static void show_prompt(void) {
    usb_cdc_puts("> ");
    s_prompt_shown = true;
}

static void process_line(void) {
    s_line[s_length] = '\0';

    if (strcmp(s_line, ".help") == 0) {
        usb_cdc_puts(".help  .reset  JavaScript expressions\r\n");
    } else if (strcmp(s_line, ".reset") == 0) {
        usb_cdc_puts("Resetting...\r\n");
        usb_cdc_reset_usb(20);
    } else if (s_length > 0) {
        char result[REPL_RESULT_SIZE] = {0};
        js_result_t status = js_engine_exec(s_line, s_length, result, sizeof(result));
        if (status == JS_OK) {
            if (result[0] != '\0') {
                usb_cdc_puts(result);
                usb_cdc_puts("\r\n");
            }
        } else {
            char error[REPL_RESULT_SIZE] = {0};
            js_engine_get_error(error, sizeof(error));
            usb_cdc_puts("Error: ");
            usb_cdc_puts(error);
            usb_cdc_puts("\r\n");
        }
    }

    s_length = 0;
    s_line[0] = '\0';
    show_prompt();
}

void repl_init(void) {
    s_length = 0;
    s_line[0] = '\0';
    s_initialized = true;
    s_prompt_shown = false;
    s_ignore_lf = false;
}

void repl_task(void) {
    if (!s_initialized) {
        return;
    }

    while (usb_cdc_available() > 0) {
        int input = usb_cdc_getchar();
        if (input < 0) {
            break;
        }

        char c = (char)input;
        if (!s_prompt_shown) {
            show_prompt();
        }

        if (c == '\n' && s_ignore_lf) {
            s_ignore_lf = false;
            continue;
        }
        s_ignore_lf = false;

        if (c == '\r' || c == '\n') {
            if (c == '\r') {
                s_ignore_lf = true;
            }
            usb_cdc_puts("\r\n");
            process_line();
        } else if (c == 0x03) {
            usb_cdc_puts("^C\r\n");
            repl_reset();
            show_prompt();
        } else if (c == 0x08 || c == 0x7f) {
            if (s_length > 0) {
                s_length--;
                s_line[s_length] = '\0';
                usb_cdc_puts("\b \b");
            }
        } else if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f &&
                   s_length < REPL_LINE_SIZE - 1) {
            s_line[s_length++] = c;
            usb_cdc_putchar(c);
        }
    }
}

bool repl_is_multiline(void) {
    return false;
}

void repl_reset(void) {
    s_length = 0;
    s_line[0] = '\0';
}
