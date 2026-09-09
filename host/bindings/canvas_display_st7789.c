/* ST7789 profiles: Waveshare 1.47-A V3 and Touch LCD 2.8 T3.
 * One retained native-endian RGB565 surface per display; no DMA or scratch
 * full-frame copy. All entry points run on the serialized JS task thread.
 */
#include "canvas_display.h"
#include "canvas_spi.h"
#include "pico/stdlib.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct st7789 {
    struct st7789 *next;
    canvas_lcd_config_t config;
    canvas_spi_t bus;
    uint16_t *pixels;
} st7789_t;

/* Only live LCD connections are tracked, not a general bus/pin manager. */
static st7789_t *live_displays;

static bool connection_conflicts(const canvas_lcd_config_t *c) {
    const int pins[] = {c->sck, c->mosi, c->cs, c->dc, c->reset, c->backlight};
    for (const st7789_t *it = live_displays; it; it = it->next) {
        const canvas_lcd_config_t *old = &it->config;
        bool shared_bus = c->spi == old->spi;
        if (shared_bus && (c->sck != old->sck || c->mosi != old->mosi)) return true;
        const int used[] = {old->sck, old->mosi, old->cs, old->dc, old->reset, old->backlight};
        for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
            if (pins[i] < 0) continue;
            for (size_t j = 0; j < sizeof(used) / sizeof(used[0]); ++j) {
                if (pins[i] == used[j] && !(shared_bus && i < 2 && j < 2)) return true;
            }
        }
    }
    return false;
}

static bool valid_config(const canvas_lcd_config_t *c) {
    if (!c || (c->profile != CANVAS_PANEL_WAVESHARE_1_47 &&
               c->profile != CANVAS_PANEL_WAVESHARE_2_8) || c->width <= 0 || c->height <= 0 || c->x_offset < 0 || c->y_offset < 0)
        return false;
    /* MADCTL exchanges the controller's 240x320 address axes. */
    int max_x = c->horizontal ? 320 : 240;
    int max_y = c->horizontal ? 240 : 320;
    if (c->width > max_x || c->height > max_y ||
        c->x_offset > max_x - c->width || c->y_offset > max_y - c->height)
        return false;
    if ((size_t)c->width > SIZE_MAX / sizeof(uint16_t) / (size_t)c->height)
        return false;
    const int pins[] = {c->sck, c->mosi, c->cs, c->dc, c->reset, c->backlight};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (pins[i] < (i >= 4 ? -1 : 0) || pins[i] >= NUM_BANK0_GPIOS) return false;
        if (pins[i] < 0) continue;
        for (size_t j = 0; j < i; ++j)
            if (pins[i] == pins[j]) return false;
    }
    return true;
}

static void output(int pin, bool value) {
    if (pin < 0) return;
    gpio_init((uint)pin);
    gpio_put((uint)pin, value); /* Establish inactive value before output enable. */
    gpio_set_dir((uint)pin, GPIO_OUT);
}

/* A register and its parameters share CS. Controller state is borrowed only
 * for actual traffic, never during panel reset/sleep-out delays. */
static bool command(st7789_t *lcd, uint8_t cmd, const uint8_t *bytes, size_t count) {
    if (!canvas_spi_begin(&lcd->bus)) return false;
    gpio_put((uint)lcd->config.dc, false);
    gpio_put((uint)lcd->config.cs, false);
    bool ok = canvas_spi_write(&lcd->bus, &cmd, 1);
    if (ok && count) {
        gpio_put((uint)lcd->config.dc, true);
        ok = canvas_spi_write(&lcd->bus, bytes, count);
    }
    gpio_put((uint)lcd->config.cs, true);
    canvas_spi_end(&lcd->bus);
    return ok;
}

static uint16_t *acquire(canvas_display_t *display) {
    st7789_t *lcd = display->state;
    return lcd ? lcd->pixels : NULL;
}

static bool present(canvas_display_t *display) {
    st7789_t *lcd = display->state;
    if (!lcd) return false;
    const canvas_lcd_config_t *c = &lcd->config;
    unsigned x_end = (unsigned)(c->x_offset + c->width - 1);
    unsigned y_end = (unsigned)(c->y_offset + c->height - 1);
    const uint8_t columns[] = {(uint8_t)(c->x_offset >> 8), (uint8_t)c->x_offset,
        (uint8_t)(x_end >> 8), (uint8_t)x_end};
    const uint8_t rows[] = {(uint8_t)(c->y_offset >> 8), (uint8_t)c->y_offset,
        (uint8_t)(y_end >> 8), (uint8_t)y_end};
    if (!command(lcd, 0x2a, columns, sizeof(columns)) ||
        !command(lcd, 0x2b, rows, sizeof(rows)) || !canvas_spi_begin(&lcd->bus))
        return false;
    gpio_put((uint)c->dc, false);
    gpio_put((uint)c->cs, false);
    const uint8_t memory_write = 0x2c;
    bool ok = canvas_spi_write(&lcd->bus, &memory_write, 1);
    gpio_put((uint)c->dc, true);
    const size_t total = (size_t)c->width * (size_t)c->height;
    uint8_t chunk[256];
    for (size_t pos = 0; ok && pos < total;) {
        size_t count = total - pos;
        if (count > sizeof(chunk) / 2) count = sizeof(chunk) / 2;
        for (size_t i = 0; i < count; ++i) {
            uint16_t pixel = lcd->pixels[pos + i];
            /* T3 factory RAMCTRL (B0 00 E8) uses little-endian RGB565;
             * the 1.47 V3 uses high-byte first. Never mutate native pixels. */
            bool little = c->profile == CANVAS_PANEL_WAVESHARE_2_8;
            chunk[2 * i] = little ? (uint8_t)pixel : (uint8_t)(pixel >> 8);
            chunk[2 * i + 1] = little ? (uint8_t)(pixel >> 8) : (uint8_t)pixel;
        }
        ok = canvas_spi_write(&lcd->bus, chunk, count * 2);
        pos += count;
    }
    gpio_put((uint)c->cs, true);
    canvas_spi_end(&lcd->bus);
    if (ok && c->backlight >= 0) gpio_put((uint)c->backlight, true);
    return ok;
}

static void release_pins(const canvas_lcd_config_t *c) {
    /* Park the dedicated outputs inactive. Do not touch shared SCK/MOSI or
     * deinitialize their controller. CS must not float and select a closed LCD. */
    gpio_put((uint)c->cs, true);
    if (c->backlight >= 0) gpio_put((uint)c->backlight, false);
    gpio_deinit((uint)c->dc);
    if (c->reset >= 0) gpio_deinit((uint)c->reset);
}

static void release(canvas_display_t *display) {
    st7789_t *lcd = display->state;
    if (!lcd) return;
    st7789_t **link = &live_displays;
    while (*link && *link != lcd) link = &(*link)->next;
    if (*link) *link = lcd->next;
    release_pins(&lcd->config);
    free(lcd->pixels);
    free(lcd);
    display->state = NULL;
}

/* Register values/rotation from Waveshare's RP2350-Touch-LCD-2.8 demo,
 * C/libraries/bsp/bsp_st7789.c. Keep its native-endian RAMCTRL mode. */
static bool initialize_panel_28(st7789_t *lcd) {
    const canvas_lcd_config_t *c=&lcd->config;
    if(c->reset>=0) {
        gpio_put((uint)c->reset,false);sleep_ms(50);
        gpio_put((uint)c->reset,true);sleep_ms(50);
    } else {
        if(!command(lcd,0x01,NULL,0))return false;
        sleep_ms(150);
    }
    if(!command(lcd,0x29,NULL,0))return false;
    sleep_ms(10);
    if(!command(lcd,0x11,NULL,0))return false;
    sleep_ms(120); /* Conservative sleep-out wait, rather than vendor's 10 ms. */
    const uint8_t madctl=c->horizontal?0x60:0x00;
    if(!command(lcd,0x36,&madctl,1))return false;
    static const struct {uint8_t cmd,count,data[14];} init[]={
        {0x3a,1,{0x05}}, {0xb0,2,{0x00,0xe8}},
        {0xb2,5,{0x0c,0x0c,0x00,0x33,0x33}},
        {0xb7,1,{0x75}}, {0xbb,1,{0x1a}}, {0xc0,1,{0x2c}},
        {0xc2,2,{0x01,0xff}}, {0xc3,1,{0x13}}, {0xc4,1,{0x20}},
        {0xc6,1,{0x0f}}, {0xd0,2,{0xa4,0xa1}}, {0xd6,1,{0xa1}},
        {0xe0,14,{0xd0,0x0d,0x14,0x0d,0x0d,0x09,0x38,0x44,0x4e,0x3a,0x17,0x18,0x2f,0x30}},
        {0xe1,14,{0xd0,0x09,0x0f,0x08,0x07,0x14,0x37,0x44,0x4d,0x38,0x15,0x16,0x2c,0x2e}},
        {0x21,0,{0}}, {0x29,0,{0}}, {0x2c,0,{0}}
    };
    for(size_t i=0;i<sizeof(init)/sizeof(init[0]);i++)
        if(!command(lcd,init[i].cmd,init[i].data,init[i].count))return false;
    sleep_ms(20);
    return true;
}

static bool initialize_panel(st7789_t *lcd) {
    const canvas_lcd_config_t *c = &lcd->config;
    output(c->cs, true);
    output(c->dc, false);
    output(c->backlight, false);
    output(c->reset, true);
    if(c->profile == CANVAS_PANEL_WAVESHARE_2_8) return initialize_panel_28(lcd);
    if (c->reset >= 0) {
        sleep_ms(100);
        gpio_put((uint)c->reset, false);
        sleep_ms(100);
        gpio_put((uint)c->reset, true);
        sleep_ms(100);
    } else {
        if (!command(lcd, 0x01, NULL, 0)) return false;
        sleep_ms(150);
    }
    if (!command(lcd, 0x11, NULL, 0)) return false;
    sleep_ms(120);
    const uint8_t madctl = c->horizontal ? 0x70 : 0x00;
    if (!command(lcd, 0x36, &madctl, 1)) return false;
    /* Matches examples/waveshare-lcd-1.47/st7789v3.js. */
    static const struct { uint8_t cmd, count, data[14]; } init[] = {
        {0x3a, 1, {0x05}},
        {0xb2, 5, {0x0c, 0x0c, 0x00, 0x33, 0x33}},
        {0xb7, 1, {0x35}}, {0xbb, 1, {0x35}}, {0xc0, 1, {0x2c}},
        {0xc2, 1, {0x01}}, {0xc3, 1, {0x13}}, {0xc4, 1, {0x20}},
        {0xc6, 1, {0x0f}}, {0xd0, 2, {0xa4, 0xa1}}, {0xd6, 1, {0xa1}},
        {0xe0, 14, {0xf0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3e, 0x38, 0x12, 0x12, 0x28, 0x30}},
        {0xe1, 14, {0xf0, 0x07, 0x0a, 0x0d, 0x0b, 0x07, 0x28, 0x33, 0x3e, 0x36, 0x14, 0x14, 0x29, 0x32}},
        {0x21, 0, {0}}, {0x11, 0, {0}}
    };
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); ++i)
        if (!command(lcd, init[i].cmd, init[i].data, init[i].count)) return false;
    sleep_ms(120);
    if (!command(lcd, 0x29, NULL, 0)) return false;
    sleep_ms(20);
    return true;
}

bool canvas_display_st7789_init(canvas_display_t *display, const canvas_lcd_config_t *config) {
    canvas_spi_t bus;
    if (!display || display->state || !valid_config(config) || connection_conflicts(config) ||
        !canvas_spi_configure(&bus, config->spi, config->sck, config->mosi, config->baudrate))
        return false;
    st7789_t *lcd = calloc(1, sizeof(*lcd));
    if (!lcd) return false;
    lcd->config = *config;
    lcd->bus = bus;
    lcd->bus.mode = config->profile == CANVAS_PANEL_WAVESHARE_2_8 ? 3 : 0;
    lcd->pixels = calloc((size_t)config->width * (size_t)config->height, sizeof(uint16_t));
    if (!lcd->pixels) { free(lcd); return false; }
    /* All allocation succeeds before the first GPIO/controller write. */
    if (!initialize_panel(lcd)) {
        release_pins(config);
        free(lcd->pixels);
        free(lcd);
        return false;
    }
    lcd->next = live_displays;
    live_displays = lcd;
    display->width = config->width;
    display->height = config->height;
    display->state = lcd;
    display->acquire = acquire;
    display->present = present;
    display->release = release;
    return true;
}
