#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int pins[49],live,bus_owned,fail_spi,stuck,busy_ticks;static unsigned usb_ticks;static int64_t now;
static unsigned command_count,updates;static uint8_t last_command,ram[5000];static size_t ram_len;
void vTaskDelay(unsigned ms){now+=(int64_t)ms*1000;}
int64_t esp_timer_get_time(void){return now;}
int gpio_get_level(int p){assert(p==8);if(stuck)return 1;if(busy_ticks){busy_ticks--;return 1;}return 0;}
int gpio_set_level(int p,int v){pins[p]=v;return 0;}
int gpio_config(const gpio_config_t *c){assert(!(c->pin_bit_mask&((1ULL<<19)|(1ULL<<20))));return 0;}
int esp_task_wdt_status(void *p){(void)p;return 0;}int esp_task_wdt_reset(void){return 0;}
void usb_cdc_puts(const char *s){assert(strstr(s,"timeout"));}void usb_cdc_task(void){usb_ticks++;}
int spi_bus_initialize(int host,const spi_bus_config_t *b,int dma){assert(host==1&&dma==1&&b->mosi_io_num==13&&b->sclk_io_num==12&&b->miso_io_num==-1);bus_owned++;return 0;}
int spi_bus_add_device(int h,const spi_device_interface_config_t *d,spi_device_handle_t *s){assert(h==1&&d->mode==0&&d->clock_speed_hz==4000000&&d->spics_io_num==-1);*s=(void*)1;return 0;}
int spi_bus_remove_device(spi_device_handle_t s){assert(s);return 0;}int spi_bus_free(int h){assert(h==1);bus_owned--;return 0;}
void *heap_caps_malloc(size_t n,int caps){assert(n==80000&&caps==3);live++;return malloc(n);}
static void tracked_free(void *p){if(p)live--;free(p);}
int spi_device_polling_transmit(spi_device_handle_t s,spi_transaction_t *t){
 assert(s&&!pins[11]&&pins[6]==0);if(fail_spi)return -1;
 const uint8_t *p=t->tx_buffer;size_t n=t->length/8;
 if(!pins[10]){assert(n==1);last_command=*p;command_count++;if(*p==0x20){updates++;busy_ticks=2;}}
 else if(last_command==0x24){assert(ram_len+n<=5000);memcpy(ram+ram_len,p,n);ram_len+=n;}
 else if(last_command==0x45){assert(n==4&&p[0]==199&&p[1]==0&&p[2]==0&&p[3]==0);}
 else if(last_command==0x10){assert(n==1&&p[0]==1);}
 return 0;
}
#define free tracked_free
#include "../platform/esp32/main/canvas_display_epaper154.c"
#undef free
int main(void){
 canvas_display_t d={0};assert(canvas_display_epaper154_init(&d));assert(d.width==200&&d.height==200&&pins[6]==1&&command_count==0&&live==1);
 canvas_display_t second={0};assert(!canvas_display_epaper154_init(&second));
 uint16_t *p=d.acquire(&d);assert(p[0]==0xffff&&p[39999]==0xffff);p[0]=0xf800;p[1]=0x07e0;p[2]=0x001f;p[39999]=0;
 assert(d.present(&d));assert(ram_len==5000&&ram[0]==0x5f&&ram[4999]==0xfe&&last_command==0x10&&pins[6]==1&&usb_ticks>0&&updates==2);
 d.release(&d);assert(live==0&&bus_owned==0);
 assert(canvas_display_epaper154_init(&d));stuck=1;int64_t before=now;assert(!d.present(&d));assert(now-before>=15000000&&now-before<16000000&&pins[6]==1);d.release(&d);stuck=0;
 assert(canvas_display_epaper154_init(&d));fail_spi=1;assert(!d.present(&d));assert(pins[6]==1);d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper: no refresh on open, RGB565 threshold/bit order, full frame, BUSY wait/timeout, sleep/power-off, transfer failure, ownership and release");
}
