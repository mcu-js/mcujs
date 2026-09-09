/* V2 vendor sequence: see ../EPAPER154.md. Private SPI, no JS transport API. */
#include "canvas_display.h"
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
#define DC 10
#define CS 11
#define SCK 12
#define MOSI 13
#define RST 9
#define BUSY 8
#define POWER 6
static spi_device_handle_t spi;
static bool owned;
static const uint8_t lut[159] = {
 0x80,0x48,0x40,0,0,0,0,0,0,0,0,0,
 0x40,0x48,0x80,0,0,0,0,0,0,0,0,0,
 0x80,0x48,0x40,0,0,0,0,0,0,0,0,0,
 0x40,0x48,0x80,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
 10,0,0,0,0,0,0, 8,1,0,8,1,0,2, 10,0,0,0,0,0,0,
 0,0,0,0,0,0,0, 0,0,0,0,0,0,0, 0,0,0,0,0,0,0,
 0,0,0,0,0,0,0, 0,0,0,0,0,0,0, 0,0,0,0,0,0,0,
 0,0,0,0,0,0,0, 0,0,0,0,0,0,0, 0,0,0,0,0,0,0,
 0x22,0x22,0x22,0x22,0x22,0x22,0,0,0, 0x22,0x17,0x41,0,0x32,0x20
};
static void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1); }
static bool idle(void) {
 int64_t end=esp_timer_get_time()+15000000;
 while(gpio_get_level(BUSY)) {
  if(esp_timer_get_time()>=end) {usb_cdc_puts("ePaper BUSY timeout\r\n");return false;}
  usb_cdc_task();
  if(esp_task_wdt_status(NULL)==ESP_OK) esp_task_wdt_reset();
  delay_ms(5);
 }
 return true;
}
static bool send(bool data,const uint8_t *bytes,size_t n) {
 gpio_set_level(DC,data);gpio_set_level(CS,0);
 spi_transaction_t t={.length=n*8,.tx_buffer=bytes};
 bool ok=spi_device_polling_transmit(spi,&t)==ESP_OK;
 gpio_set_level(CS,1);return ok;
}
static bool cmd(uint8_t c,const uint8_t *data,size_t n) {
 return send(false,&c,1) && (!n || send(true,data,n));
}
#define C(c, ...) cmd(c,(const uint8_t[]){__VA_ARGS__},sizeof((const uint8_t[]){__VA_ARGS__}))
static uint16_t *acquire(canvas_display_t *d) {return d->state;}
static bool present(canvas_display_t *d) {
 gpio_set_level(POWER,0);delay_ms(20);
 gpio_set_level(RST,1);delay_ms(50);gpio_set_level(RST,0);delay_ms(20);gpio_set_level(RST,1);delay_ms(50);
 bool ok=idle() && cmd(0x12,NULL,0) && idle() && C(0x01,0xc7,0,1) && C(0x11,1)
  && C(0x44,0,24) && C(0x45,199,0,0,0) && C(0x3c,1) && C(0x18,0x80)
  && C(0x22,0xb1) && cmd(0x20,NULL,0) && C(0x4e,0) && C(0x4f,199,0) && idle()
  && cmd(0x32,lut,153) && idle() && C(0x3f,lut[153]) && C(0x03,lut[154])
  && C(0x04,lut[155],lut[156],lut[157]) && C(0x2c,lut[158]) && cmd(0x24,NULL,0);
 uint16_t *pixels=d->state;
 for(int y=0;ok && y<200;y++) {
  uint8_t row[25]={0};
  for(int x=0;x<200;x++) {
   uint16_t v=pixels[y*200+x];
   unsigned r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
   if(299*r+587*g+114*b>=128000)row[x/8]|=0x80>>(x%8);
  }
  ok=send(true,row,sizeof(row));
 }
 ok=ok && C(0x22,0xc7) && cmd(0x20,NULL,0);
 delay_ms(10); /* Allow BUSY to assert before observing completion. */
 ok=ok && idle();
 if(ok)ok=C(0x10,1); /* SSD1681 deep sleep; reset required next update. */
 delay_ms(10);
 gpio_set_level(POWER,1);
 return ok;
}
static void release(canvas_display_t *d) {
 gpio_set_level(POWER,1);
 if(spi){spi_bus_remove_device(spi);spi=NULL;spi_bus_free(SPI2_HOST);}
 free(d->state);d->state=NULL;owned=false;
}
bool canvas_display_epaper154_init(canvas_display_t *d) {
 if(owned)return false;
 uint16_t *pixels=heap_caps_malloc(200*200*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!pixels)return false;
 memset(pixels,0xff,200*200*2);
 gpio_config_t out={.pin_bit_mask=(1ULL<<DC)|(1ULL<<CS)|(1ULL<<RST)|(1ULL<<POWER),.mode=GPIO_MODE_OUTPUT};
 gpio_config_t in={.pin_bit_mask=1ULL<<BUSY,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 if(gpio_config(&out)!=ESP_OK || gpio_config(&in)!=ESP_OK){free(pixels);return false;}
 gpio_set_level(POWER,1);gpio_set_level(CS,1);
 spi_bus_config_t bus={.mosi_io_num=MOSI,.miso_io_num=-1,.sclk_io_num=SCK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=200};
 spi_device_interface_config_t dev={.clock_speed_hz=4000000,.mode=0,.spics_io_num=-1,.queue_size=1};
 if(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK){free(pixels);return false;}
 if(spi_bus_add_device(SPI2_HOST,&dev,&spi)!=ESP_OK){spi_bus_free(SPI2_HOST);free(pixels);return false;}
 owned=true;d->width=200;d->height=200;d->state=pixels;d->acquire=acquire;d->present=present;d->release=release;
 return true;
}
