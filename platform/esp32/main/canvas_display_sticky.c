/* Private SSD1677 full-frame backend. See ../STICKY.md for vendor provenance. */
#include "canvas_display.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#if !defined(CONFIG_SPIRAM) || !defined(CONFIG_SPIRAM_MODE_OCT) || !defined(CONFIG_SPIRAM_BOOT_INIT) || !defined(CONFIG_SPIRAM_USE_MALLOC)
#error "Sticky Canvas requires initialized octal PSRAM"
#endif
#endif
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_cdc.h"
#include <stdlib.h>
#include <string.h>

#define DC 16
#define CS 15
#define SCK 13
#define MOSI 14
#define RST 17
#define BUSY 18
#define POWER 47
#define WIDTH 800
#define HEIGHT 480
static spi_device_handle_t spi;
static bool owned;

static uint16_t *acquire(canvas_display_t *d) {return d->state;}
static void delay_ms(unsigned ms) {
    vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
}
static void service(void) {
    usb_cdc_task();
    if (esp_task_wdt_status(NULL)==ESP_OK) (void)esp_task_wdt_reset();
}
static bool idle(void) {
    int64_t deadline=esp_timer_get_time()+10000000;
    while (gpio_get_level(BUSY)) {
        if (esp_timer_get_time()>=deadline) {
            usb_cdc_puts("Sticky ePaper BUSY timeout\r\n");return false;
        }
        service();delay_ms(5);
    }
    return true;
}
static bool send(bool data,const uint8_t *bytes,size_t n) {
    if (gpio_set_level(DC,data)!=ESP_OK || gpio_set_level(CS,0)!=ESP_OK) return false;
    spi_transaction_t t={.length=n*8,.tx_buffer=bytes};
    bool ok=spi_device_polling_transmit(spi,&t)==ESP_OK;
    return gpio_set_level(CS,1)==ESP_OK && ok;
}
static bool cmd(uint8_t c,const uint8_t *data,size_t n) {
    return send(false,&c,1) && (!n || send(true,data,n));
}
#define C(c, ...) cmd(c,(const uint8_t[]){__VA_ARGS__},sizeof((const uint8_t[]){__VA_ARGS__}))
static bool write_plane(uint8_t command,const uint16_t *pixels) {
    if (!C(0x4e,0,0) || !C(0x4f,0,0) || !cmd(command,NULL,0)) return false;
    for (unsigned y=0;y<HEIGHT;y++) {
        uint8_t row[WIDTH/8]={0};
        for (unsigned x=0;x<WIDTH;x++) {
            /* Seeed dashboard's landscape orientation: rotate180 + mirrorX. */
            uint16_t v=pixels[(HEIGHT-1-y)*WIDTH+x];
            unsigned r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
            if (299*r+587*g+114*b>=128000) row[x/8]|=0x80>>(x%8);
        }
        /* 100-byte rows exceed IDF's non-DMA FIFO; send two 50-byte chunks. */
        if (!send(true,row,50) || !send(true,row+50,50)) return false;
        if ((y&15)==0) {service();delay_ms(1);}
    }
    return true;
}
static bool present(canvas_display_t *d) {
    bool ok=gpio_set_level(POWER,1)==ESP_OK;
    delay_ms(100);
    ok=ok && gpio_set_level(RST,0)==ESP_OK;
    delay_ms(10);
    ok=ok && gpio_set_level(RST,1)==ESP_OK;
    delay_ms(10);
    ok=ok && idle() && cmd(0x12,NULL,0);
    delay_ms(10); /* Software reset and refresh need time for BUSY to assert. */
    ok=ok && idle() && C(0x18,0x80) && C(0x3c,1)
        && C(0x0c,0xae,0xc7,0xc3,0xc0,0x80) && C(0x01,0xdf,1,2)
        && C(0x11,3) && C(0x44,0,0,0x1f,3) && C(0x45,0,0,0xdf,1)
        && idle() && write_plane(0x26,d->state) && write_plane(0x24,d->state)
        && C(0x22,0xf7) && cmd(0x20,NULL,0);
    delay_ms(10);
    ok=ok && idle();
    if (ok) ok=C(0x10,3); /* SSD1677 deep sleep; next present resets it. */
    delay_ms(100);
    /* Always remove the rail, even after a timeout/failed transaction. */
    bool powered_off=gpio_set_level(POWER,0)==ESP_OK;
    return ok && powered_off;
}
#undef C
static void release(canvas_display_t *d) {
    (void)gpio_set_level(POWER,0);
    if (spi) {
        (void)spi_bus_remove_device(spi);spi=NULL;
        (void)spi_bus_free(SPI2_HOST);
    }
    free(d->state);d->state=NULL;owned=false;
}
bool canvas_display_sticky_init(canvas_display_t *d) {
    if (!d || owned) return false;
    uint16_t *pixels=heap_caps_malloc(WIDTH*HEIGHT*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!pixels) return false;
    memset(pixels,0xff,WIDTH*HEIGHT*sizeof(uint16_t));
    gpio_config_t out={.pin_bit_mask=(1ULL<<DC)|(1ULL<<CS)|(1ULL<<RST)|(1ULL<<POWER),.mode=GPIO_MODE_OUTPUT};
    gpio_config_t in={.pin_bit_mask=1ULL<<BUSY,.mode=GPIO_MODE_INPUT};
    if (gpio_set_level(POWER,0)!=ESP_OK || gpio_set_level(CS,1)!=ESP_OK ||
        gpio_config(&out)!=ESP_OK || gpio_config(&in)!=ESP_OK) {
        free(pixels);return false;
    }
    /* Polling without DMA: every transfer is <=64 bytes, the IDF S3 FIFO limit.
     * No PSRAM buffer is passed to SPI. Unused data pins MUST be -1. */
    spi_bus_config_t bus={.mosi_io_num=MOSI,.miso_io_num=-1,.sclk_io_num=SCK,
        .quadwp_io_num=-1,.quadhd_io_num=-1,
#ifdef ESP_PLATFORM
        .data4_io_num=-1,.data5_io_num=-1,.data6_io_num=-1,.data7_io_num=-1,
#endif
        .max_transfer_sz=64};
    spi_device_interface_config_t dev={.clock_speed_hz=10000000,.mode=0,.spics_io_num=-1,.queue_size=1};
    if (spi_bus_initialize(SPI2_HOST,&bus,0)!=ESP_OK) {free(pixels);return false;}
    if (spi_bus_add_device(SPI2_HOST,&dev,&spi)!=ESP_OK) {
        spi=NULL;(void)spi_bus_free(SPI2_HOST);free(pixels);return false;
    }
    owned=true;d->width=WIDTH;d->height=HEIGHT;d->state=pixels;
    d->acquire=acquire;d->present=present;d->release=release;
    return true;
}
