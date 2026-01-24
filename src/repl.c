/**
 * mcujs - JavaScript Runtime for Raspberry Pi Pico
 * REPL (Read-Eval-Print Loop) Implementation
 */

#include "repl.h"
#include "usb_cdc.h"
#include "engine.h"
#include "fs.h"
#include "board.h"
#include "board_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// REPL configuration
#define REPL_MAX_LINE_LENGTH 256
#define REPL_HISTORY_SIZE 8
#define REPL_PROMPT "> "
#define REPL_CONTINUATION_PROMPT "... "
#define REPL_MAX_COMPLETIONS 16
#define REPL_MAX_COMPLETION_LEN 32
#define REPL_PASTE_BUFFER_SIZE 4096
#define REPL_PASTE_PROMPT "m> "

// Special characters
#define CHAR_BACKSPACE 0x08
#define CHAR_DELETE 0x7F
#define CHAR_ENTER_CR 0x0D
#define CHAR_ENTER_LF 0x0A
#define CHAR_ESCAPE 0x1B
#define CHAR_CTRL_C 0x03
#define CHAR_CTRL_D 0x04
#define CHAR_TAB 0x09

// Escape sequence state
typedef enum {
    ESC_NONE,
    ESC_START,      // Got ESC
    ESC_BRACKET     // Got ESC [
} esc_state_t;

// REPL state
typedef struct {
    char line_buffer[REPL_MAX_LINE_LENGTH];
    uint16_t line_len;      // Total length of line
    uint16_t cursor_pos;    // Cursor position within line
    bool multiline_mode;
    uint8_t brace_depth;
    uint8_t paren_depth;
    uint8_t bracket_depth;
    bool prompt_shown;
    bool initialized;
    esc_state_t esc_state;

    bool paste_mode;
    char paste_buffer[REPL_PASTE_BUFFER_SIZE];
    uint16_t paste_len;
    char paste_path[REPL_MAX_LINE_LENGTH];

    // History
    char history[REPL_HISTORY_SIZE][REPL_MAX_LINE_LENGTH];
    uint8_t history_count;  // Number of items in history
    uint8_t history_head;   // Next slot to write
    int8_t history_pos;     // Current browsing position (-1 = current line)
    char saved_line[REPL_MAX_LINE_LENGTH];  // Saved current line when browsing history
    uint16_t saved_len;
} repl_state_t;

static repl_state_t repl_state;

// Forward declarations
static void repl_show_prompt(void);
static void repl_process_line(void);
static void repl_handle_char(char c);
static void repl_handle_command(const char* cmd);
static bool repl_check_complete(void);
static void repl_print_result(const char* result);
static void repl_print_error(const char* error);
static void repl_history_add(const char* line);
static void repl_history_prev(void);
static void repl_history_next(void);
static void repl_clear_line(void);
static void repl_set_line(const char* line);
static void repl_cursor_left(void);
static void repl_cursor_right(void);
static void repl_delete_char(void);
static void repl_insert_char(char c);
static void repl_handle_tab(void);
static bool repl_wait_for_keypress(uint32_t timeout_ms);
static void repl_paste_start(const char *path);
static void repl_paste_append_line(void);
static void repl_paste_finish(void);
static void repl_register_identifiers(const char *source, size_t length);

// Tab completion state
typedef struct {
    char matches[REPL_MAX_COMPLETIONS][REPL_MAX_COMPLETION_LEN];
    uint8_t match_count;
    uint16_t word_start;    // Start position of word being completed
} completion_state_t;

static completion_state_t completion_state;

/**
 * Initialize the REPL
 */
void repl_init(void) {
    memset(&repl_state, 0, sizeof(repl_state));
    repl_state.history_pos = -1;
    repl_state.initialized = true;
}

/**
 * Non-blocking REPL task
 */
void repl_task(void) {
    if (!repl_state.initialized) {
        return;
    }
    
    // Don't process if CDC not connected
    if (!usb_cdc_connected()) {
        repl_state.prompt_shown = false;
        return;
    }
    
    // Show prompt if needed
    if (!repl_state.prompt_shown) {
        repl_show_prompt();
        repl_state.prompt_shown = true;
    }
    
    // Process available input
    while (usb_cdc_available()) {
        int c = usb_cdc_getchar();
        if (c >= 0) {
            repl_handle_char((char)c);
        }
    }
}

/**
 * Show the REPL prompt
 */
static void repl_show_prompt(void) {
    if (repl_state.paste_mode) {
        usb_cdc_puts(REPL_PASTE_PROMPT);
    } else if (repl_state.multiline_mode) {
        usb_cdc_puts(REPL_CONTINUATION_PROMPT);
    } else {
        usb_cdc_puts(REPL_PROMPT);
    }
}

/**
 * Clear current line on terminal
 */
static void repl_clear_line(void) {
    // Move cursor to start of line content (after prompt)
    while (repl_state.cursor_pos > 0) {
        usb_cdc_puts("\b");
        repl_state.cursor_pos--;
    }
    // Overwrite with spaces
    for (uint16_t i = 0; i < repl_state.line_len; i++) {
        usb_cdc_puts(" ");
    }
    // Move back to start
    for (uint16_t i = 0; i < repl_state.line_len; i++) {
        usb_cdc_puts("\b");
    }
    repl_state.line_len = 0;
    repl_state.line_buffer[0] = '\0';
}

/**
 * Set line content and display it
 */
static void repl_set_line(const char* line) {
    repl_clear_line();
    size_t len = strlen(line);
    if (len >= REPL_MAX_LINE_LENGTH) {
        len = REPL_MAX_LINE_LENGTH - 1;
    }
    memcpy(repl_state.line_buffer, line, len);
    repl_state.line_buffer[len] = '\0';
    repl_state.line_len = len;
    repl_state.cursor_pos = len;
    usb_cdc_puts(repl_state.line_buffer);
}

/**
 * Move cursor left
 */
static void repl_cursor_left(void) {
    if (repl_state.cursor_pos > 0) {
        repl_state.cursor_pos--;
        usb_cdc_puts("\b");
    }
}

/**
 * Move cursor right
 */
static void repl_cursor_right(void) {
    if (repl_state.cursor_pos < repl_state.line_len) {
        char c[2] = {repl_state.line_buffer[repl_state.cursor_pos], '\0'};
        usb_cdc_puts(c);
        repl_state.cursor_pos++;
    }
}

/**
 * Delete character at cursor (or before cursor for backspace)
 */
static void repl_delete_char(void) {
    if (repl_state.cursor_pos > 0) {
        repl_state.cursor_pos--;
        // Shift remaining characters left
        memmove(&repl_state.line_buffer[repl_state.cursor_pos],
                &repl_state.line_buffer[repl_state.cursor_pos + 1],
                repl_state.line_len - repl_state.cursor_pos);
        repl_state.line_len--;
        
        // Update display: backspace, print rest of line, space to clear last char, move back
        usb_cdc_puts("\b");
        usb_cdc_puts(&repl_state.line_buffer[repl_state.cursor_pos]);
        usb_cdc_puts(" ");
        // Move cursor back to position
        for (uint16_t i = repl_state.cursor_pos; i <= repl_state.line_len; i++) {
            usb_cdc_puts("\b");
        }
    }
}

/**
 * Insert character at cursor position
 */
static void repl_insert_char(char c) {
    if (repl_state.line_len >= REPL_MAX_LINE_LENGTH - 1) {
        return;
    }
    
    if (repl_state.cursor_pos == repl_state.line_len) {
        // Append at end
        repl_state.line_buffer[repl_state.line_len++] = c;
        repl_state.line_buffer[repl_state.line_len] = '\0';
        repl_state.cursor_pos++;
        char echo[2] = {c, '\0'};
        usb_cdc_puts(echo);
    } else {
        // Insert in middle - shift characters right
        memmove(&repl_state.line_buffer[repl_state.cursor_pos + 1],
                &repl_state.line_buffer[repl_state.cursor_pos],
                repl_state.line_len - repl_state.cursor_pos + 1);
        repl_state.line_buffer[repl_state.cursor_pos] = c;
        repl_state.line_len++;
        
        // Print from cursor to end
        usb_cdc_puts(&repl_state.line_buffer[repl_state.cursor_pos]);
        repl_state.cursor_pos++;
        // Move cursor back to position
        for (uint16_t i = repl_state.cursor_pos; i < repl_state.line_len; i++) {
            usb_cdc_puts("\b");
        }
    }
}

/**
 * Add line to history
 */
static void repl_history_add(const char* line) {
    if (line[0] == '\0') return;  // Don't add empty lines
    
    // Don't add if same as last entry
    if (repl_state.history_count > 0) {
        uint8_t last = (repl_state.history_head + REPL_HISTORY_SIZE - 1) % REPL_HISTORY_SIZE;
        if (strcmp(repl_state.history[last], line) == 0) {
            return;
        }
    }
    
    strncpy(repl_state.history[repl_state.history_head], line, REPL_MAX_LINE_LENGTH - 1);
    repl_state.history[repl_state.history_head][REPL_MAX_LINE_LENGTH - 1] = '\0';
    repl_state.history_head = (repl_state.history_head + 1) % REPL_HISTORY_SIZE;
    if (repl_state.history_count < REPL_HISTORY_SIZE) {
        repl_state.history_count++;
    }
}

/**
 * Navigate to previous history entry (up arrow)
 */
static void repl_history_prev(void) {
    if (repl_state.history_count == 0) return;
    
    if (repl_state.history_pos == -1) {
        // Save current line before browsing history
        memcpy(repl_state.saved_line, repl_state.line_buffer, REPL_MAX_LINE_LENGTH);
        repl_state.saved_len = repl_state.line_len;
        repl_state.history_pos = 0;
    } else if (repl_state.history_pos < repl_state.history_count - 1) {
        repl_state.history_pos++;
    } else {
        return;  // Already at oldest entry
    }
    
    // Get entry from history (newest is at history_head - 1)
    int idx = (repl_state.history_head - 1 - repl_state.history_pos + REPL_HISTORY_SIZE) % REPL_HISTORY_SIZE;
    repl_set_line(repl_state.history[idx]);
}

/**
 * Navigate to next history entry (down arrow)
 */
static void repl_history_next(void) {
    if (repl_state.history_pos == -1) return;  // Already at current line
    
    repl_state.history_pos--;
    
    if (repl_state.history_pos == -1) {
        // Restore saved current line
        repl_set_line(repl_state.saved_line);
    } else {
        int idx = (repl_state.history_head - 1 - repl_state.history_pos + REPL_HISTORY_SIZE) % REPL_HISTORY_SIZE;
        repl_set_line(repl_state.history[idx]);
    }
}

/**
 * Handle a single character input
 */
static void repl_handle_char(char c) {
    // Handle escape sequences
    if (repl_state.esc_state == ESC_START) {
        if (c == '[') {
            repl_state.esc_state = ESC_BRACKET;
            return;
        } else {
            repl_state.esc_state = ESC_NONE;
            // Fall through to handle character normally
        }
    } else if (repl_state.esc_state == ESC_BRACKET) {
        repl_state.esc_state = ESC_NONE;
        switch (c) {
            case 'A':  // Up arrow
                repl_history_prev();
                return;
            case 'B':  // Down arrow
                repl_history_next();
                return;
            case 'C':  // Right arrow
                repl_cursor_right();
                return;
            case 'D':  // Left arrow
                repl_cursor_left();
                return;
            case 'H':  // Home
                while (repl_state.cursor_pos > 0) {
                    repl_cursor_left();
                }
                return;
            case 'F':  // End
                while (repl_state.cursor_pos < repl_state.line_len) {
                    repl_cursor_right();
                }
                return;
            default:
                return;  // Ignore unknown escape sequences
        }
    }
    
    switch (c) {
        case CHAR_ENTER_CR:
        case CHAR_ENTER_LF:
            usb_cdc_puts("\r\n");
            repl_process_line();
            break;
            
        case CHAR_BACKSPACE:
        case CHAR_DELETE:
            repl_delete_char();
            break;
            
        case CHAR_CTRL_C:
            // Cancel current input
            usb_cdc_puts("^C\r\n");
            repl_reset();
            repl_show_prompt();
            break;
            
        case CHAR_CTRL_D:
            // EOF - ignore in REPL (no exit on embedded)
            break;
            
        case CHAR_ESCAPE:
            repl_state.esc_state = ESC_START;
            break;
            
        case CHAR_TAB:
            repl_handle_tab();
            break;
            
        default:
            // Regular character - add to buffer if printable
            if (c >= 0x20 && c < 0x7F) {
                repl_insert_char(c);
            }
            break;
    }
}

/**
 * Process a complete line of input
 */
static void repl_process_line(void) {
    if (repl_state.paste_mode) {
        if (repl_state.line_len == 4 && strcmp(repl_state.line_buffer, ".end") == 0) {
            repl_paste_finish();
        } else {
            repl_paste_append_line();
        }
        repl_state.line_len = 0;
        repl_state.cursor_pos = 0;
        repl_state.line_buffer[0] = '\0';
        repl_show_prompt();
        return;
    }

    // Trim trailing whitespace
    while (repl_state.line_len > 0 && 
           (repl_state.line_buffer[repl_state.line_len - 1] == ' ' ||
            repl_state.line_buffer[repl_state.line_len - 1] == '\t')) {
        repl_state.line_len--;
        repl_state.line_buffer[repl_state.line_len] = '\0';
    }
    
    // Empty line
    if (repl_state.line_len == 0) {
        repl_show_prompt();
        return;
    }
    
    // Check for REPL commands (start with .)
    if (repl_state.line_buffer[0] == '.') {
        const char *command = repl_state.line_buffer + 1;
        const char *path = NULL;
        size_t prefix_len = 0;

        if (strncmp(command, "multiline", 9) == 0 &&
            (command[9] == '\0' || command[9] == ' ')) {
            prefix_len = 9;
        }

        if (prefix_len > 0) {
            path = command + prefix_len;
            while (*path == ' ') {
                path++;
            }
            repl_history_add(repl_state.line_buffer);
            repl_reset();
            repl_paste_start(path[0] != '\0' ? path : NULL);
            repl_show_prompt();
            return;
        }

        repl_history_add(repl_state.line_buffer);
        repl_handle_command(command);
        repl_reset();
        repl_show_prompt();
        return;
    }
    
    // Check if input is complete (balanced braces, etc.)
    if (!repl_check_complete()) {
        // Continue on next line
        repl_state.multiline_mode = true;
        repl_state.line_buffer[repl_state.line_len++] = '\n';
        repl_state.cursor_pos = repl_state.line_len;
        repl_show_prompt();
        return;
    }
    
    // Add to history before execution
    repl_history_add(repl_state.line_buffer);
    
    // Execute JavaScript
    char result_buf[256];
    js_result_t result = js_engine_exec(repl_state.line_buffer, 
                                         repl_state.line_len,
                                         result_buf, sizeof(result_buf));
    
    if (result == JS_OK) {
        repl_register_identifiers(repl_state.line_buffer, repl_state.line_len);
        if (result_buf[0] != '\0' && strcmp(result_buf, "undefined") != 0) {
            repl_print_result(result_buf);
        }
    } else {
        char error_buf[128];
        js_engine_get_error(error_buf, sizeof(error_buf));
        repl_print_error(error_buf);
    }
    
    repl_reset();
    repl_show_prompt();
}

/**
 * Callback for .ls directory listing
 */
static bool repl_ls_callback(const fs_entry_t *entry, void *user_data) {
    (void)user_data;
    char buf[64];
    if (entry->is_dir) {
        snprintf(buf, sizeof(buf), "  <DIR>  %s\r\n", entry->name);
    } else {
        snprintf(buf, sizeof(buf), "  %5lu  %s\r\n", (unsigned long)entry->size, entry->name);
    }
    usb_cdc_puts(buf);
    return true;  /* Continue iteration */
}

/**
 * Wait for a keypress within timeout
 */
static bool repl_wait_for_keypress(uint32_t timeout_ms) {
    const uint32_t step_ms = 50;
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        usb_cdc_task();
        if (usb_cdc_available()) {
            usb_cdc_getchar();
            return true;
        }
        board_delay_ms(step_ms);
        elapsed += step_ms;
    }

    return false;
}

static bool repl_is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == '$';
}

static bool repl_is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

static bool repl_match_keyword(const char *source, size_t len, size_t pos, const char *keyword) {
    size_t keyword_len = strlen(keyword);
    if (pos + keyword_len > len) {
        return false;
    }
    if (pos > 0 && repl_is_ident_char(source[pos - 1])) {
        return false;
    }
    if (strncmp(source + pos, keyword, keyword_len) != 0) {
        return false;
    }
    if (pos + keyword_len < len && repl_is_ident_char(source[pos + keyword_len])) {
        return false;
    }
    return true;
}

static void repl_register_identifiers(const char *source, size_t length) {
    bool in_line_comment = false;
    bool in_block_comment = false;
    char string_delim = '\0';

    for (size_t i = 0; i < length; i++) {
        char c = source[i];
        char next = (i + 1 < length) ? source[i + 1] : '\0';

        if (in_line_comment) {
            if (c == '\n') {
                in_line_comment = false;
            }
            continue;
        }
        if (in_block_comment) {
            if (c == '*' && next == '/') {
                in_block_comment = false;
                i++;
            }
            continue;
        }
        if (string_delim != '\0') {
            if (c == '\\') {
                i++;
                continue;
            }
            if (c == string_delim) {
                string_delim = '\0';
            }
            continue;
        }

        if (c == '/' && next == '/') {
            in_line_comment = true;
            i++;
            continue;
        }
        if (c == '/' && next == '*') {
            in_block_comment = true;
            i++;
            continue;
        }
        if (c == '\'' || c == '"' || c == '`') {
            string_delim = c;
            continue;
        }

        if (repl_match_keyword(source, length, i, "const") ||
            repl_match_keyword(source, length, i, "let") ||
            repl_match_keyword(source, length, i, "var")) {
            const char *keyword = repl_match_keyword(source, length, i, "const") ? "const"
                                  : repl_match_keyword(source, length, i, "let") ? "let"
                                  : "var";
            i += strlen(keyword);
            while (i < length) {
                while (i < length && isspace((unsigned char)source[i])) {
                    i++;
                }
                if (i >= length || !repl_is_ident_start(source[i])) {
                    break;
                }
                size_t start = i;
                i++;
                while (i < length && repl_is_ident_char(source[i])) {
                    i++;
                }
                char name_buf[REPL_MAX_COMPLETION_LEN];
                size_t name_len = i - start;
                if (name_len > 0 && name_len < sizeof(name_buf)) {
                    memcpy(name_buf, source + start, name_len);
                    name_buf[name_len] = '\0';
                    js_engine_register_global_identifier(name_buf);
                }
                while (i < length && isspace((unsigned char)source[i])) {
                    i++;
                }
                if (i < length && source[i] == ',') {
                    i++;
                    continue;
                }
                i--;
                break;
            }
            continue;
        }

        if (repl_match_keyword(source, length, i, "function") ||
            repl_match_keyword(source, length, i, "class")) {
            const char *keyword = repl_match_keyword(source, length, i, "function") ? "function" : "class";
            i += strlen(keyword);
            while (i < length && isspace((unsigned char)source[i])) {
                i++;
            }
            if (i < length && repl_is_ident_start(source[i])) {
                size_t start = i;
                i++;
                while (i < length && repl_is_ident_char(source[i])) {
                    i++;
                }
                char name_buf[REPL_MAX_COMPLETION_LEN];
                size_t name_len = i - start;
                if (name_len > 0 && name_len < sizeof(name_buf)) {
                    memcpy(name_buf, source + start, name_len);
                    name_buf[name_len] = '\0';
                    js_engine_register_global_identifier(name_buf);
                }
            }
            continue;
        }
    }
}

static void repl_paste_start(const char *path) {
    repl_state.paste_mode = true;
    repl_state.paste_len = 0;
    repl_state.paste_buffer[0] = '\0';
    repl_state.paste_path[0] = '\0';

    if (path && path[0] != '\0') {
        strncpy(repl_state.paste_path, path, sizeof(repl_state.paste_path) - 1);
        repl_state.paste_path[sizeof(repl_state.paste_path) - 1] = '\0';
        usb_cdc_puts("Multi-line input. End with .end. Writing to ");
        usb_cdc_puts(repl_state.paste_path);
        usb_cdc_puts("\r\n");
    } else {
        usb_cdc_puts("Multi-line input. End with .end.\r\n");
    }
}

static void repl_paste_append_line(void) {
    size_t required = repl_state.paste_len + repl_state.line_len + 1;
    if (required >= REPL_PASTE_BUFFER_SIZE) {
        usb_cdc_puts("Error: Paste buffer full\r\n");
        repl_state.paste_mode = false;
        repl_state.paste_len = 0;
        repl_state.paste_buffer[0] = '\0';
        repl_state.paste_path[0] = '\0';
        return;
    }

    memcpy(&repl_state.paste_buffer[repl_state.paste_len],
           repl_state.line_buffer,
           repl_state.line_len);
    repl_state.paste_len += repl_state.line_len;
    repl_state.paste_buffer[repl_state.paste_len++] = '\n';
    repl_state.paste_buffer[repl_state.paste_len] = '\0';
}

static void repl_paste_finish(void) {
    if (repl_state.paste_len == 0) {
        repl_state.paste_mode = false;
        return;
    }

    if (repl_state.paste_path[0] != '\0') {
        fs_invalidate();
        fs_file_t file;
        fs_result_t result = fs_open(&file, repl_state.paste_path,
                                     FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_TRUNCATE);
        if (result != FS_OK) {
            usb_cdc_puts("Error: Could not open file for paste\r\n");
        } else {
            size_t bytes_written = 0;
            result = fs_write(&file, repl_state.paste_buffer, repl_state.paste_len, &bytes_written);
            fs_close(&file);
            if (result == FS_OK && bytes_written == repl_state.paste_len) {
                fs_sync();
                fs_notify_host();
                usb_cdc_puts("Paste saved\r\n");
            } else {
                usb_cdc_puts("Error: Failed to write paste\r\n");
            }
        }
    } else {
        char result_buf[256];
        js_result_t result = js_engine_exec(repl_state.paste_buffer,
                                             repl_state.paste_len,
                                             result_buf, sizeof(result_buf));
        if (result == JS_OK) {
            repl_register_identifiers(repl_state.paste_buffer, repl_state.paste_len);
            if (result_buf[0] != '\0' && strcmp(result_buf, "undefined") != 0) {
                repl_print_result(result_buf);
            }
        } else {
            char error_buf[128];
            js_engine_get_error(error_buf, sizeof(error_buf));
            repl_print_error(error_buf);
        }
    }

    repl_state.paste_mode = false;
    repl_state.paste_len = 0;
    repl_state.paste_buffer[0] = '\0';
    repl_state.paste_path[0] = '\0';
}

/**
 * Handle REPL dot commands
 */
static void repl_handle_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        usb_cdc_puts("mcujs REPL Commands:\r\n");
        usb_cdc_puts("  .help     - Show this help message\r\n");
        usb_cdc_puts("  .info     - Show system information\r\n");
        usb_cdc_puts("  .ls       - List files on the device\r\n");
        usb_cdc_puts("  .cat FILE - Display contents of a file\r\n");
        usb_cdc_puts("  .rm FILE  - Remove a file\r\n");
        usb_cdc_puts("  .run FILE   - Execute a JavaScript file\r\n");
        usb_cdc_puts("  .multiline [FILE] - Multi-line input (end with .end)\r\n");
        usb_cdc_puts("  .format     - Format filesystem (prompted)\r\n");
        usb_cdc_puts("  .format!    - Format filesystem immediately\r\n");
        usb_cdc_puts("  .uf2        - Reboot into UF2 mode (prompted)\r\n");
        usb_cdc_puts("  .uf2!       - Reboot into UF2 mode immediately\r\n");
        usb_cdc_puts("  .usbreset   - Reset USB connection (reboot)\r\n");
        usb_cdc_puts("\r\n");
        usb_cdc_puts("JavaScript APIs:\r\n");
        usb_cdc_puts("  console.log(), console.warn(), console.error()\r\n");
        usb_cdc_puts("  setTimeout(), clearTimeout(), setInterval(), clearInterval()\r\n");
        usb_cdc_puts("  board (reset, enterUf2, millis, led, ids)\r\n");
        usb_cdc_puts("Runtime Module APIs:\r\n");
        usb_cdc_puts("  require('fs'), require('gpio'), require('pwm'), require('i2c'), require('spi'), require('adc'), require('neopixel')\r\n");
        usb_cdc_puts("Runtime APIs:\r\n");
        usb_cdc_puts("  process.version, process.versions, process.arch, process.platform\r\n");
        usb_cdc_puts("  require('mcujs:module').builtinModules\r\n");
    }
    else if (strcmp(cmd, "info") == 0) {
        const board_info_t *info = board_get_info();
        char buf[64];
        
        usb_cdc_puts("Board: ");
        usb_cdc_puts(info->name);
        usb_cdc_puts("\r\n");
        usb_cdc_puts("Chip: ");
        usb_cdc_puts(info->chip);
        usb_cdc_puts("\r\n");
        
        usb_cdc_puts("Build: ");
        usb_cdc_puts(MCUJS_BUILD_ID);
        usb_cdc_puts("\r\n");
        
        snprintf(buf, sizeof(buf), "Flash: %lu KB\r\n", (unsigned long)(info->flash_size / 1024));
        usb_cdc_puts(buf);
        
        snprintf(buf, sizeof(buf), "RAM: %lu KB\r\n", (unsigned long)(info->ram_size / 1024));
        usb_cdc_puts(buf);
        
        uint32_t free_space = fs_get_free_space();
        if (free_space < 1000) {
            snprintf(buf, sizeof(buf), "FS Free: %lu B\r\n", (unsigned long)free_space);
        } else {
            snprintf(buf, sizeof(buf), "FS Free: %lu KB\r\n", (unsigned long)(free_space / 1024));
        }
        usb_cdc_puts(buf);
    }
    else if (strcmp(cmd, "ls") == 0) {
        fs_invalidate();  /* Refresh to see USB changes */
        usb_cdc_puts("Files on device:\r\n");
        fs_result_t result = fs_list_dir("/", repl_ls_callback, NULL);
        if (result != FS_OK) {
            usb_cdc_puts("Error reading directory\r\n");
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        const char* filename = cmd + 4;
        while (*filename == ' ') filename++;
        
        fs_invalidate();  /* Refresh to see USB changes */
        fs_file_t file;
        if (fs_open(&file, filename, FS_MODE_READ) == FS_OK) {
            char buf[64];
            size_t bytes_read;
            while (fs_read(&file, buf, sizeof(buf) - 1, &bytes_read) == FS_OK && bytes_read > 0) {
                buf[bytes_read] = '\0';
                /* Convert \n to \r\n for proper terminal display */
                for (size_t i = 0; i < bytes_read; i++) {
                    if (buf[i] == '\n') {
                        usb_cdc_puts("\r\n");
                    } else {
                        char c[2] = {buf[i], '\0'};
                        usb_cdc_puts(c);
                    }
                }
            }
            usb_cdc_puts("\r\n");
            fs_close(&file);
        } else {
            usb_cdc_puts("Error: Could not open file\r\n");
        }
    }
    else if (strncmp(cmd, "rm ", 3) == 0) {
        const char* filename = cmd + 3;
        while (*filename == ' ') filename++;
        
        fs_invalidate();  /* Refresh to see USB changes */
        if (fs_remove(filename) == FS_OK) {
            usb_cdc_puts("File removed\r\n");
        } else {
            usb_cdc_puts("Error: Could not remove file\r\n");
        }
    }
    else if (strncmp(cmd, "run ", 4) == 0) {
        const char* filename = cmd + 4;
        while (*filename == ' ') filename++;
        
        fs_invalidate();  /* Refresh to see USB changes */
        js_result_t result = js_engine_exec_file(filename);
        if (result != JS_OK) {
            char error_buf[128];
            js_engine_get_error(error_buf, sizeof(error_buf));
            repl_print_error(error_buf);
        }
    }
    else if (strcmp(cmd, "format") == 0) {
        usb_cdc_puts("WARNING: This will erase all files!\r\n");
        usb_cdc_puts("Formatting in 3s. Press any key to cancel.\r\n");
        if (repl_wait_for_keypress(3000)) {
            usb_cdc_puts("Format cancelled.\r\n");
            return;
        }
        usb_cdc_puts("Formatting filesystem...\r\n");
        fs_result_t result = fs_format();
        if (result == FS_OK) {
            usb_cdc_puts("Filesystem formatted successfully.\r\n");
        } else {
            usb_cdc_puts("Format failed!\r\n");
        }
    }
    else if (strcmp(cmd, "format!") == 0) {
        usb_cdc_puts("Formatting filesystem...\r\n");
        fs_result_t result = fs_format();
        if (result == FS_OK) {
            usb_cdc_puts("Filesystem formatted successfully.\r\n");
        } else {
            usb_cdc_puts("Format failed!\r\n");
        }
    }
    else if (strcmp(cmd, "uf2") == 0) {
        usb_cdc_puts("Entering UF2 mode in 3s. Press any key to cancel.\r\n");
        if (repl_wait_for_keypress(3000)) {
            usb_cdc_puts("UF2 entry cancelled.\r\n");
            return;
        }
        usb_cdc_puts("Rebooting into UF2 mode...\r\n");
        if (!board_enter_uf2()) {
            usb_cdc_puts("UF2 boot not supported on this board.\r\n");
        }
    }
    else if (strcmp(cmd, "uf2!") == 0) {
        usb_cdc_puts("Rebooting into UF2 mode...\r\n");
        if (!board_enter_uf2()) {
            usb_cdc_puts("UF2 boot not supported on this board.\r\n");
        }
    }
    else if (strcmp(cmd, "usbreset") == 0) {
        usb_cdc_puts("Resetting USB connection...\r\n");
        usb_cdc_flush();
        usb_cdc_reset_usb(250);
    }
    else {
        usb_cdc_puts("Unknown command. Type .help for available commands.\r\n");
    }
}

/**
 * Check if current input is syntactically complete
 * (balanced braces, parentheses, brackets)
 */
static bool repl_check_complete(void) {
    int brace_depth = 0;
    int paren_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool in_template = false;
    char string_char = 0;
    bool escape_next = false;
    
    for (uint16_t i = 0; i < repl_state.line_len; i++) {
        char c = repl_state.line_buffer[i];
        
        if (escape_next) {
            escape_next = false;
            continue;
        }
        
        if (c == '\\') {
            escape_next = true;
            continue;
        }
        
        if (in_string) {
            if (c == string_char) {
                in_string = false;
            }
            continue;
        }
        
        if (in_template) {
            if (c == '`') {
                in_template = false;
            }
            // TODO: Handle ${} in template literals
            continue;
        }
        
        switch (c) {
            case '"':
            case '\'':
                in_string = true;
                string_char = c;
                break;
            case '`':
                in_template = true;
                break;
            case '{':
                brace_depth++;
                break;
            case '}':
                brace_depth--;
                break;
            case '(':
                paren_depth++;
                break;
            case ')':
                paren_depth--;
                break;
            case '[':
                bracket_depth++;
                break;
            case ']':
                bracket_depth--;
                break;
        }
    }
    
    // Input is complete if all brackets are balanced
    return (brace_depth <= 0 && paren_depth <= 0 && bracket_depth <= 0 && 
            !in_string && !in_template);
}

/**
 * Print a successful result
 * Converts \n to \r\n for proper terminal display
 */
static void repl_print_result(const char* result) {
    while (*result) {
        if (*result == '\n') {
            usb_cdc_puts("\r\n");
        } else {
            char c[2] = {*result, '\0'};
            usb_cdc_puts(c);
        }
        result++;
    }
    usb_cdc_puts("\r\n");
}

/**
 * Print an error message
 */
static void repl_print_error(const char* error) {
    usb_cdc_puts("Error: ");
    usb_cdc_puts(error);
    usb_cdc_puts("\r\n");
}

/**
 * Check if REPL is in multi-line mode
 */
bool repl_is_multiline(void) {
    return repl_state.multiline_mode;
}

/**
 * Reset REPL state
 */
void repl_reset(void) {
    repl_state.line_len = 0;
    repl_state.cursor_pos = 0;
    repl_state.line_buffer[0] = '\0';
    repl_state.multiline_mode = false;
    repl_state.brace_depth = 0;
    repl_state.paren_depth = 0;
    repl_state.bracket_depth = 0;
    repl_state.history_pos = -1;
    repl_state.esc_state = ESC_NONE;
    repl_state.paste_mode = false;
    repl_state.paste_len = 0;
    repl_state.paste_buffer[0] = '\0';
    repl_state.paste_path[0] = '\0';
}

/**
 * Callback for collecting completions
 */
static bool completion_callback(const char *name, void *user_data) {
    (void)user_data;
    
    if (completion_state.match_count >= REPL_MAX_COMPLETIONS) {
        return false;  // Stop iteration
    }
    
    strncpy(completion_state.matches[completion_state.match_count], 
            name, REPL_MAX_COMPLETION_LEN - 1);
    completion_state.matches[completion_state.match_count][REPL_MAX_COMPLETION_LEN - 1] = '\0';
    completion_state.match_count++;
    
    return true;  // Continue iteration
}

/**
 * Find longest common prefix among all matches
 */
static size_t find_common_prefix(void) {
    if (completion_state.match_count == 0) {
        return 0;
    }
    if (completion_state.match_count == 1) {
        return strlen(completion_state.matches[0]);
    }
    
    size_t prefix_len = 0;
    while (1) {
        char c = completion_state.matches[0][prefix_len];
        if (c == '\0') break;
        
        bool all_match = true;
        for (uint8_t i = 1; i < completion_state.match_count; i++) {
            if (completion_state.matches[i][prefix_len] != c) {
                all_match = false;
                break;
            }
        }
        
        if (!all_match) break;
        prefix_len++;
    }
    
    return prefix_len;
}

/**
 * Handle tab key for completion
 */
static void repl_handle_tab(void) {
    /* Find the word start (for completion) */
    uint16_t word_start = repl_state.cursor_pos;
    while (word_start > 0) {
        char c = repl_state.line_buffer[word_start - 1];
        /* Valid identifier chars or dot for property access */
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '$') {
            word_start--;
        } else {
            break;
        }
    }
    
    /* No word to complete */
    if (word_start == repl_state.cursor_pos) {
        return;
    }
    
    /* Extract the partial word */
    char partial[64];
    size_t partial_len = repl_state.cursor_pos - word_start;
    if (partial_len >= sizeof(partial)) {
        partial_len = sizeof(partial) - 1;
    }
    memcpy(partial, &repl_state.line_buffer[word_start], partial_len);
    partial[partial_len] = '\0';
    
    /* Reset completion state and get completions */
    completion_state.match_count = 0;
    completion_state.word_start = word_start;
    
    js_engine_get_completions(partial, completion_callback, NULL);
    
    if (completion_state.match_count == 0) {
        /* No matches - do nothing */
        return;
    }
    
    /* Find the part after the last dot (what we're actually completing) */
    const char *dot = strrchr(partial, '.');
    const char *completing = dot ? (dot + 1) : partial;
    size_t completing_len = strlen(completing);
    
    if (completion_state.match_count == 1) {
        /* Single match - complete it */
        const char *match = completion_state.matches[0];
        size_t match_len = strlen(match);
        
        /* Insert the remaining characters */
        for (size_t i = completing_len; i < match_len; i++) {
            repl_insert_char(match[i]);
        }
    } else {
        /* Multiple matches - find common prefix and complete to that */
        size_t common_len = find_common_prefix();
        
        if (common_len > completing_len) {
            /* Complete to the common prefix */
            const char *match = completion_state.matches[0];
            for (size_t i = completing_len; i < common_len; i++) {
                repl_insert_char(match[i]);
            }
        } else {
            /* Show all matches on new line */
            usb_cdc_puts("\r\n");
            
            for (uint8_t i = 0; i < completion_state.match_count; i++) {
                usb_cdc_puts(completion_state.matches[i]);
                usb_cdc_puts("  ");
            }
            usb_cdc_puts("\r\n");
            
            /* Redraw prompt and current line */
            repl_show_prompt();
            usb_cdc_puts(repl_state.line_buffer);
            
            /* Move cursor back to correct position */
            for (uint16_t i = repl_state.cursor_pos; i < repl_state.line_len; i++) {
                usb_cdc_puts("\b");
            }
        }
    }
}
