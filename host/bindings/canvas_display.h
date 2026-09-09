/* Private display-instance boundary. Pixels are packed native-endian RGB565. */
#ifndef MCUJS_CANVAS_DISPLAY_H
#define MCUJS_CANVAS_DISPLAY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct canvas_display canvas_display_t;
struct canvas_display {
    int width, height;
    bool pending, closed;
    void *state;
    uint16_t *(*acquire)(canvas_display_t *);
    bool (*present)(canvas_display_t *);
    void (*release)(canvas_display_t *);
    canvas_display_t *next;
};
typedef struct {
    int spi, sck, mosi, cs, dc, reset, backlight;
    int width, height, x_offset, y_offset, baudrate;
    bool horizontal;
    enum { CANVAS_PANEL_WAVESHARE_1_47=0, CANVAS_PANEL_WAVESHARE_2_8=1, CANVAS_PANEL_WAVESHARE_1_69=2 } profile;
} canvas_lcd_config_t;
bool canvas_display_epaper154_init(canvas_display_t *display);
bool canvas_display_dvi_init(canvas_display_t *display);
bool canvas_display_st7789_init(canvas_display_t *display, const canvas_lcd_config_t *config);
#endif
