/* Host-only SDK stand-ins: these tests do not qualify physical hardware. */
#include "canvas_display.h"
#include "fake_sdk.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

spi_inst_t fake_spi[2] = {{.id = 0}, {.id = 1}};
resets_hw_t fake_resets;
static bool pins[NUM_BANK0_GPIOS], dirs[NUM_BANK0_GPIOS];
static gpio_function_t funcs[NUM_BANK0_GPIOS];
static unsigned hardware_calls, alloc_calls, fail_alloc, live_allocs;
static bool busy[2], readable[2];
static int fail_write = -1;
static unsigned expected_mode;
static size_t write_calls, max_write;
typedef struct { uint8_t value, dc, bus, cs; } wire_byte;
static wire_byte wire[300000];
static size_t wire_size;
static uint delays[8];
static size_t delay_count;
static int chip_selects[4] = {17, 9, 13, 5};
static int data_commands[4] = {16, 8, 12, 4};

void *__real_calloc(size_t count, size_t size);
void __real_free(void *ptr);
void *__wrap_calloc(size_t count, size_t size) {
    if (++alloc_calls == fail_alloc) return NULL;
    void *ptr = __real_calloc(count, size);
    if (ptr) ++live_allocs;
    return ptr;
}
void __wrap_free(void *ptr) {
    if (ptr) { assert(live_allocs); --live_allocs; }
    __real_free(ptr);
}
bool spi_is_busy(spi_inst_t *spi) { return busy[spi->id]; }
bool spi_is_readable(spi_inst_t *spi) { return readable[spi->id]; }
uint32_t clock_get_hz(clock_index clk) { (void)clk; return 150000000; }
uint spi_set_baudrate(spi_inst_t *spi, uint baud) {
    ++hardware_calls;
    spi->hw.cpsr = 2;
    spi->hw.cr0 = (spi->hw.cr0 & 255u) | ((150000000u / (2 * baud) - 1) << 8);
    return baud;
}
void spi_set_format(spi_inst_t *spi, uint bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t order) {
    ++hardware_calls;
    assert(order == SPI_MSB_FIRST);
    spi->hw.cr0 = (spi->hw.cr0 & ~255u) | (bits - 1) | (cpol << 6) | (cpha << 7);
}
uint spi_init(spi_inst_t *spi, uint baud) {
    ++hardware_calls;
    memset(&spi->hw, 0, sizeof(spi->hw));
    fake_resets.reset &= ~(spi->id ? RESETS_RESET_SPI1_BITS : RESETS_RESET_SPI0_BITS);
    spi->hw.cr1 = SPI_SSPCR1_SSE_BITS;
    spi->hw.dmacr = 3;
    return spi_set_baudrate(spi, baud);
}
void spi_deinit(spi_inst_t *spi) {
    ++hardware_calls;
    fake_resets.reset |= spi->id ? RESETS_RESET_SPI1_BITS : RESETS_RESET_SPI0_BITS;
    memset(&spi->hw, 0, sizeof(spi->hw));
}
int spi_write_blocking(spi_inst_t *spi, const uint8_t *data, size_t len) {
    ++hardware_calls;
    assert((spi->hw.cr0 & 255u) == (expected_mode == 3 ? 0xc7u : 7u));
    assert(spi->hw.cr1 == SPI_SSPCR1_SSE_BITS);
    assert(spi->hw.dmacr == 0 && spi->hw.imsc == 0);
    assert(funcs[spi->id ? 10 : 18] == GPIO_FUNC_SPI);
    if (len > max_write) max_write = len;
    if ((int)write_calls++ == fail_write) return -1;
    int selected = -1;
    for (int n = 0; n < 4; ++n) {
        if (dirs[chip_selects[n]] && !pins[chip_selects[n]]) {
            assert(selected == -1); selected = n;
        }
    }
    assert(selected != -1 && wire_size + len <= sizeof(wire) / sizeof(wire[0]));
    for (size_t i = 0; i < len; ++i)
        wire[wire_size++] = (wire_byte){data[i], pins[data_commands[selected]], spi->id, chip_selects[selected]};
    return (int)len;
}
void gpio_init(uint pin) { ++hardware_calls; assert(pin < NUM_BANK0_GPIOS); funcs[pin] = GPIO_FUNC_SIO; pins[pin] = dirs[pin] = false; }
void gpio_deinit(uint pin) { ++hardware_calls; funcs[pin] = GPIO_FUNC_NULL; dirs[pin] = false; }
void gpio_put(uint pin, bool value) { ++hardware_calls; pins[pin] = value; }
void gpio_set_dir(uint pin, bool output) { ++hardware_calls; dirs[pin] = output; }
void gpio_set_function(uint pin, gpio_function_t fn) { ++hardware_calls; funcs[pin] = fn; }
gpio_function_t gpio_get_function(uint pin) { return funcs[pin]; }
void sleep_ms(uint ms) {
    if (delay_count < sizeof(delays) / sizeof(delays[0])) delays[delay_count] = ms;
    ++delay_count;
}

static canvas_lcd_config_t panel(void) {
    return (canvas_lcd_config_t){.spi = 0, .sck = 18, .mosi = 19, .cs = 17,
        .dc = 16, .reset = 20, .backlight = 21, .width = 320, .height = 172,
        .x_offset = 0, .y_offset = 34, .baudrate = 37500000, .horizontal = true};
}
static void clear_wire(void) { wire_size = write_calls = max_write = 0; }
static void test_initialization(void) {
    canvas_display_t d = {0};
    canvas_lcd_config_t c = panel();
    clear_wire(); delay_count = 0;
    assert(canvas_display_st7789_init(&d, &c));
    /* Literal reference bytes from the checked-in Waveshare JS example. */
    static const uint8_t expected[] = {
        0x11, 0x36,0x70, 0x3a,0x05, 0xb2,0x0c,0x0c,0x00,0x33,0x33,
        0xb7,0x35, 0xbb,0x35, 0xc0,0x2c, 0xc2,0x01, 0xc3,0x13,
        0xc4,0x20, 0xc6,0x0f, 0xd0,0xa4,0xa1, 0xd6,0xa1,
        0xe0,0xf0,0x00,0x04,0x04,0x04,0x05,0x29,0x33,0x3e,0x38,0x12,0x12,0x28,0x30,
        0xe1,0xf0,0x07,0x0a,0x0d,0x0b,0x07,0x28,0x33,0x3e,0x36,0x14,0x14,0x29,0x32,
        0x21, 0x11, 0x29
    };
    assert(wire_size == sizeof(expected));
    for (size_t i = 0; i < sizeof(expected); ++i) assert(wire[i].value == expected[i]);
    const uint expected_delays[] = {100, 100, 100, 120, 120, 20};
    assert(delay_count == sizeof(expected_delays) / sizeof(expected_delays[0]));
    for (size_t i = 0; i < delay_count; ++i) assert(delays[i] == expected_delays[i]);
    assert(!pins[21] && pins[20] && pins[17]);
    d.release(&d);
    c.horizontal = false; c.width = 172; c.height = 320;
    c.x_offset = 34; c.y_offset = 0;
    clear_wire(); assert(canvas_display_st7789_init(&d, &c));
    assert(wire[1].value == 0x36 && wire[2].value == 0x00);
    d.release(&d); assert(!live_allocs);
}
static void test_frame(void) {
    canvas_display_t d = {0};
    canvas_lcd_config_t c = panel();
    assert(canvas_display_st7789_init(&d, &c));
    assert(d.width == 320 && d.height == 172);
    uint16_t *pixels = d.acquire(&d);
    assert(pixels && pixels[0] == 0 && pixels[55039] == 0);
    pixels[0] = 0xf800; pixels[1] = 0x07e0; pixels[2] = 0x001f;
    pixels[55039] = 0xa55a;
    assert(d.acquire(&d) == pixels);
    clear_wire();
    assert(d.present(&d));
    assert(pins[21]);
    const uint8_t expected[] = {0x2a,0,0,1,0x3f,0x2b,0,0x22,0,0xcd,0x2c,0xf8,0,7,0xe0,0,0x1f};
    assert(wire_size == 110091);
    for (size_t n = 0; n < sizeof(expected); ++n) {
        assert(wire[n].value == expected[n]);
        assert(wire[n].dc == (n != 0 && n != 5 && n != 10));
        assert(wire[n].bus == 0 && wire[n].cs == 17);
    }
    assert(max_write <= 256);
    assert(wire[wire_size - 2].value == 0xa5 && wire[wire_size - 1].value == 0x5a);
    assert(pixels[55039] == 0xa55a);
    assert(pixels[0] == 0xf800 && pixels[1] == 0x07e0 && pixels[2] == 0x001f);
    assert(pins[17]);
    d.release(&d);
    assert(!d.state && !live_allocs);
}
static void reject_unchanged(canvas_lcd_config_t c) {
    canvas_display_t d = {0};
    unsigned old_calls = hardware_calls, old_live = live_allocs;
    assert(!canvas_display_st7789_init(&d, &c));
    assert(!d.state && live_allocs == old_live && hardware_calls == old_calls);
}
static void test_connections(void) {
    canvas_lcd_config_t a = panel(), b = panel(), c = panel();
    canvas_display_t da = {0}, db = {0}, dc = {0};
    assert(canvas_display_st7789_init(&da, &a));
    reject_unchanged(a); /* Same SPI/CS cannot silently drive an existing LCD. */
    b.cs = 9; b.dc = 8; b.reset = 22; b.backlight = 23;
    canvas_lcd_config_t bad = b;
    bad.dc = a.reset; reject_unchanged(bad);
    bad = b; bad.cs = a.mosi; reject_unchanged(bad);
    bad = b; bad.sck = 2; bad.mosi = 3; reject_unchanged(bad);
    bad = b; bad.spi = 1; bad.sck = 10; bad.mosi = 11; bad.dc = a.cs;
    reject_unchanged(bad);
    b.horizontal = false; b.width = 172; b.height = 320;
    b.x_offset = 34; b.y_offset = 0;
    assert(canvas_display_st7789_init(&db, &b));
    assert(da.acquire(&da) != db.acquire(&db));
    da.acquire(&da)[0] = 0xf800; db.acquire(&db)[0] = 0x001f;
    clear_wire(); assert(db.present(&db));
    const uint8_t portrait[] = {0x2a,0,0x22,0,0xcd,0x2b,0,0,1,0x3f,0x2c,0,0x1f};
    for (size_t i = 0; i < sizeof(portrait); ++i) assert(wire[i].value == portrait[i]);
    for (size_t i = 0; i < wire_size; ++i) assert(wire[i].bus == 0 && wire[i].cs == 9);
    assert(da.acquire(&da)[0] == 0xf800 && db.acquire(&db)[0] == 0x001f);
    da.release(&da);
    clear_wire(); assert(db.present(&db)); /* Closing a peer leaves bus usable. */
    assert(canvas_display_st7789_init(&da, &a)); /* Claim can be reused after close. */
    c.spi = 1; c.sck = 10; c.mosi = 11; c.cs = 13; c.dc = 12;
    c.reset = 14; c.backlight = 15;
    assert(canvas_display_st7789_init(&dc, &c));
    clear_wire(); assert(dc.present(&dc));
    for (size_t i = 0; i < wire_size; ++i) assert(wire[i].bus == 1 && wire[i].cs == 13);
    da.release(&da); db.release(&db); dc.release(&dc);
    assert(live_allocs == 0);
}
static void expect_bus(spi_hw_t expected, gpio_function_t sck, gpio_function_t mosi) {
    assert(fake_spi[0].hw.cr0 == expected.cr0);
    assert(fake_spi[0].hw.cr1 == expected.cr1);
    assert(fake_spi[0].hw.cpsr == expected.cpsr);
    assert(fake_spi[0].hw.dmacr == expected.dmacr);
    assert(fake_spi[0].hw.imsc == expected.imsc);
    assert(funcs[18] == sck && funcs[19] == mosi);
    assert(!(fake_resets.reset & RESETS_RESET_SPI0_BITS));
    assert(pins[17]);
}
static void test_restore_and_failures(void) {
    canvas_lcd_config_t c = panel();
    canvas_display_t d = {0};
    spi_hw_t previous = {.cr0 = 0xabcful, .cr1 = 2, .cpsr = 42, .dmacr = 3, .imsc = 7};
    fake_spi[0].hw = previous;
    funcs[18] = GPIO_FUNC_SIO; funcs[19] = GPIO_FUNC_NULL;
    clear_wire(); assert(canvas_display_st7789_init(&d, &c));
    size_t initialization_writes = write_calls;
    expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL);
    /* Do not just restore the state seen at display creation: another bus
     * consumer is free to change its format/baud between display frames. */
    previous = (spi_hw_t){.cr0 = 0x12cf, .cr1 = 0, .cpsr = 10, .dmacr = 1, .imsc = 2};
    fake_spi[0].hw = previous;
    clear_wire(); assert(d.present(&d));
    expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL);
    const int failures[] = {0, 1, 2, 3, 4, 5, 12, 434};
    for (size_t n = 0; n < sizeof(failures) / sizeof(failures[0]); ++n) {
        clear_wire(); fail_write = failures[n];
        assert(!d.present(&d));
        expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL);
        assert(d.acquire(&d) != NULL);
    }
    fail_write = -1;
    busy[0] = true; clear_wire(); assert(!d.present(&d)); assert(!wire_size);
    expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL); busy[0] = false;
    readable[0] = true; assert(!d.present(&d)); assert(!wire_size);
    expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL); readable[0] = false;
    clear_wire(); assert(d.present(&d));
    d.release(&d); d.release(&d); /* Idempotent private cleanup. */
    assert(!d.acquire(&d) && !d.present(&d) && !live_allocs);
    expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL);

    for (unsigned n = 1; n <= 2; ++n) {
        unsigned old_calls = hardware_calls;
        fail_alloc = alloc_calls + n;
        assert(!canvas_display_st7789_init(&d, &c));
        assert(!d.state && !live_allocs && old_calls == hardware_calls);
    }
    fail_alloc = 0;
    for (int n = 0; n < (int)initialization_writes; ++n) {
        clear_wire(); fail_write = n;
        assert(!canvas_display_st7789_init(&d, &c));
        assert(!d.state && !live_allocs && !pins[21]);
        expect_bus(previous, GPIO_FUNC_SIO, GPIO_FUNC_NULL);
    }
    fail_write = -1;
    clear_wire(); assert(canvas_display_st7789_init(&d, &c)); d.release(&d);
    assert(!live_allocs);

    /* A bus initially in reset is borrowed, then put back in reset. */
    fake_resets.reset |= RESETS_RESET_SPI0_BITS;
    memset(&fake_spi[0].hw, 0, sizeof(fake_spi[0].hw));
    clear_wire(); assert(canvas_display_st7789_init(&d, &c));
    assert(fake_resets.reset & RESETS_RESET_SPI0_BITS);
    clear_wire(); assert(d.present(&d));
    assert(fake_resets.reset & RESETS_RESET_SPI0_BITS);
    assert(funcs[18] == GPIO_FUNC_SIO && funcs[19] == GPIO_FUNC_NULL);
    d.release(&d); assert(!live_allocs);
    fake_resets.reset = 0;
}
static void test_validation(void) {
    canvas_display_t d = {0};
    canvas_lcd_config_t c = panel(), bad;
    assert(!canvas_display_st7789_init(NULL, &c));
    assert(!canvas_display_st7789_init(&d, NULL));
    bad = c; bad.width = 0; reject_unchanged(bad);
    bad = c; bad.height = -1; reject_unchanged(bad);
    bad = c; bad.width = 321; reject_unchanged(bad);
    bad = c; bad.x_offset = 2147483647; reject_unchanged(bad);
    bad = c; bad.y_offset = 69; reject_unchanged(bad);
    bad = c; bad.x_offset = -1; reject_unchanged(bad);
    bad = c; bad.spi = 2; reject_unchanged(bad);
    bad = c; bad.sck = 19; reject_unchanged(bad);
    bad = c; bad.mosi = 11; reject_unchanged(bad);
    bad = c; bad.cs = -1; reject_unchanged(bad);
    bad = c; bad.dc = 30; reject_unchanged(bad);
    bad = c; bad.reset = -2; reject_unchanged(bad);
    bad = c; bad.cs = bad.reset; reject_unchanged(bad);
    bad = c; bad.baudrate = 0; reject_unchanged(bad);
    bad = c; bad.baudrate = 1; reject_unchanged(bad);
    assert(!live_allocs);
}
static void test_partial_chunk(void) {
    canvas_display_t d = {0};
    canvas_lcd_config_t c = panel(); c.width = 3; c.height = 1;
    clear_wire(); assert(canvas_display_st7789_init(&d, &c));
    uint16_t *pixels = d.acquire(&d);
    pixels[0] = 0xffff; pixels[1] = 0x1234; pixels[2] = 0xabcd;
    clear_wire(); assert(d.present(&d));
    const uint8_t expected[] = {0x2a,0,0,0,2,0x2b,0,0x22,0,0x22,0x2c,0xff,0xff,0x12,0x34,0xab,0xcd};
    assert(wire_size == sizeof(expected));
    for (size_t n = 0; n < sizeof(expected); ++n) assert(wire[n].value == expected[n]);
    assert(pixels[1] == 0x1234 && pixels[2] == 0xabcd);
    d.release(&d); assert(!live_allocs);
}
static void test_panel_28(void) {
    canvas_display_t d={0};
    canvas_lcd_config_t c=panel();
    c.profile=CANVAS_PANEL_WAVESHARE_2_8;c.spi=1;c.sck=10;c.mosi=11;
    c.cs=13;c.dc=14;c.reset=15;c.backlight=16;c.width=320;c.height=240;
    c.x_offset=0;c.y_offset=0;
    expected_mode=3;data_commands[2]=14;clear_wire();
    spi_hw_t original=fake_spi[1].hw;
    assert(canvas_display_st7789_init(&d,&c));
    assert(memcmp(&original,&fake_spi[1].hw,sizeof(original))==0);
    assert(!pins[16]);
    /* Literal panel sequence from Waveshare bsp_st7789.c, rotated landscape. */
    const uint8_t init[]={0x29,0x11,0x36,0x60,0x3a,0x05,0xb0,0,0xe8,
        0xb2,0x0c,0x0c,0,0x33,0x33,0xb7,0x75,0xbb,0x1a,0xc0,0x2c,
        0xc2,0x01,0xff,0xc3,0x13,0xc4,0x20,0xc6,0x0f,0xd0,0xa4,0xa1,0xd6,0xa1,
        0xe0,0xd0,0x0d,0x14,0x0d,0x0d,0x09,0x38,0x44,0x4e,0x3a,0x17,0x18,0x2f,0x30,
        0xe1,0xd0,0x09,0x0f,0x08,0x07,0x14,0x37,0x44,0x4d,0x38,0x15,0x16,0x2c,0x2e,
        0x21,0x29,0x2c};
    assert(wire_size==sizeof(init));
    for(size_t i=0;i<sizeof(init);i++)assert(wire[i].value==init[i]);
    uint16_t *pixels=d.acquire(&d);pixels[0]=0xf800;pixels[1]=0x07e0;pixels[2]=0x001f;
    clear_wire();assert(d.present(&d));assert(pins[16]);
    const uint8_t prefix[]={0x2a,0,0,1,0x3f,0x2b,0,0,0,0xef,0x2c,0,0xf8,0xe0,7,0x1f,0};
    assert(wire_size==153611);
    for(size_t i=0;i<sizeof(prefix);i++)assert(wire[i].value==prefix[i]);
    assert(pixels[0]==0xf800 && pixels[1]==0x07e0 && pixels[2]==0x001f);
    assert(memcmp(&original,&fake_spi[1].hw,sizeof(original))==0);
    d.release(&d);assert(!live_allocs);
    c.horizontal=false;c.width=240;c.height=320;
    clear_wire();assert(canvas_display_st7789_init(&d,&c));assert(wire[3].value==0);
    clear_wire();assert(d.present(&d));
    const uint8_t portrait[]={0x2a,0,0,0,0xef,0x2b,0,0,1,0x3f,0x2c};
    for(size_t i=0;i<sizeof(portrait);i++)assert(wire[i].value==portrait[i]);
    d.release(&d);assert(!live_allocs);
    data_commands[2]=12;expected_mode=0;
}

int main(void) {
    test_initialization();
    test_frame();
    test_connections();
    test_restore_and_failures();
    test_validation();
    test_partial_chunk();
    test_panel_28();
    puts("canvas ST7789 host tests: PASS (Waveshare init, RGB565/wire bytes, both windows, 256-byte chunks,");
    puts("  shared/independent buses, conflicts, register/mux/reset restoration, OOM,");
    puts("  transfer failures, busy/unread bus refusal, validation, release/reopen)");
    return 0;
}
