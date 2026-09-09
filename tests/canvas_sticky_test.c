/* Real driver with IDF boundary fakes; register literals from Seeed's demo. */
#include "canvas_epaper_stubs/fake_idf.h"
#include "canvas_display.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int pins[49], live, bus_owned, fail_alloc, fail_gpio, fail_bus, fail_add;
static int fail_spi, stuck, busy_ticks;
static unsigned transfers, services, watchdogs, updates, sleeps;
static int64_t now;
static uint8_t command, control, ram[48000], old_ram[48000];
static size_t ram_len, old_len;
void vTaskDelay(unsigned ms) {now+=(int64_t)ms*1000;}
int64_t esp_timer_get_time(void) {return now;}
int gpio_get_level(int p) {assert(p==18);if(stuck)return 1;if(busy_ticks){busy_ticks--;return 1;}return 0;}
int gpio_set_level(int p,int v) {if(fail_gpio)return -1;pins[p]=v;return 0;}
int gpio_config(const gpio_config_t *c) {assert(!(c->pin_bit_mask & ((1ULL<<19)|(1ULL<<20))));return fail_gpio?-1:0;}
int esp_task_wdt_status(void *p) {(void)p;return 0;}
int esp_task_wdt_reset(void) {watchdogs++;return 0;}
void usb_cdc_puts(const char *s) {assert(strstr(s,"timeout") && strstr(s,"\r\n"));}
void usb_cdc_task(void) {services++;}
int spi_bus_initialize(int h,const spi_bus_config_t *b,int dma) {
 assert(h==1 && dma==0 && b->sclk_io_num==13 && b->mosi_io_num==14 && b->miso_io_num==-1);
 assert(b->quadwp_io_num==-1 && b->quadhd_io_num==-1 && b->max_transfer_sz==64);
 if(fail_bus)return -1;
 bus_owned++;return 0;
}
int spi_bus_add_device(int h,const spi_device_interface_config_t *d,spi_device_handle_t *s) {
 assert(h==1 && d->mode==0 && d->clock_speed_hz==10000000 && d->spics_io_num==-1);
 if(fail_add)return -1;
 *s=(void*)1;return 0;
}
int spi_bus_remove_device(spi_device_handle_t s) {assert(s);return 0;}
int spi_bus_free(int h) {assert(h==1);bus_owned--;return 0;}
void *heap_caps_malloc(size_t n,int caps) {assert(n==768000 && caps==3);if(fail_alloc)return NULL;live++;return malloc(n);}
static void tracked_free(void *p) {if(p)live--;free(p);}
int spi_device_polling_transmit(spi_device_handle_t s,spi_transaction_t *t) {
 assert(s && !pins[15] && pins[47]==1);size_t n=t->length/8;assert(n>0 && n<=64);
 transfers++;if(fail_spi && transfers==(unsigned)fail_spi)return -1;
 const uint8_t *p=t->tx_buffer;
 if(!pins[16]) {
  assert(n==1);command=*p;
  if(command==0x24)ram_len=0;
  if(command==0x26)old_len=0;
  if(command==0x20){assert(control==0xf7);updates++;busy_ticks=3;}
 } else switch(command) {
  case 0x24: assert(ram_len+n<=48000);memcpy(ram+ram_len,p,n);ram_len+=n;break;
  case 0x26: assert(old_len+n<=48000);memcpy(old_ram+old_len,p,n);old_len+=n;break;
  case 0x01: assert(n==3 && !memcmp(p,(uint8_t[]){0xdf,1,2},3));break;
  case 0x0c: assert(n==5 && !memcmp(p,(uint8_t[]){0xae,0xc7,0xc3,0xc0,0x80},5));break;
  case 0x11: assert(n==1 && p[0]==3);break;
  case 0x18: assert(n==1 && p[0]==0x80);break;
  case 0x3c: assert(n==1 && p[0]==1);break;
  case 0x44: assert(n==4 && !memcmp(p,(uint8_t[]){0,0,0x1f,3},4));break;
  case 0x45: assert(n==4 && !memcmp(p,(uint8_t[]){0,0,0xdf,1},4));break;
  case 0x4e: case 0x4f: assert(n==2 && p[0]==0 && p[1]==0);break;
  case 0x22: assert(n==1 && p[0]==0xf7);control=*p;break;
  case 0x10: assert(n==1 && p[0]==3);sleeps++;break;
  default: assert(!"unexpected data register");
 }
 return 0;
}
#define free tracked_free
#include "../platform/esp32/main/canvas_display_sticky.c"
#undef free
int main(void) {
 canvas_display_t d={0}, second={0};
 assert(canvas_display_sticky_init(&d));assert(d.width==800 && d.height==480 && pins[47]==0 && live==1 && transfers==0);
 assert(!canvas_display_sticky_init(&second));
 uint16_t *p=d.acquire(&d);assert(p[0]==0xffff && p[383999]==0xffff);
 /* Dashboard landscape: 180-degree rotation followed by controller mirror-X,
  * so logical top row is last in RAM; MSB-first with white=1. */
 p[0]=0xf800;p[1]=0x07e0;p[2]=0x001f;p[7]=0;p[8]=0;p[799]=0;p[383999]=0;
 assert(d.present(&d));assert(ram_len==48000 && old_len==48000 && !memcmp(ram,old_ram,48000));
 assert(ram[47900]==0x5e && ram[47901]==0x7f && ram[47999]==0xfe && ram[99]==0xfe);
 assert(updates==1 && sleeps==1 && command==0x10 && !pins[47] && services && watchdogs);
 assert(d.present(&d));assert(updates==2 && sleeps==2); /* Always a full refresh. */
 stuck=1;int64_t before=now;unsigned count=updates;
 assert(!d.present(&d));assert(now-before>=10000000 && now-before<11000000 && !pins[47] && updates==count);stuck=0;
 fail_spi=transfers+30;assert(!d.present(&d));assert(!pins[47] && updates==count);fail_spi=0;
 assert(d.present(&d));assert(updates==count+1);
 d.release(&d);assert(live==0 && bus_owned==0 && d.state==NULL && !pins[47]);
 int *failures[]={&fail_alloc,&fail_gpio,&fail_bus,&fail_add};
 for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++) {
  *failures[i]=1;assert(!canvas_display_sticky_init(&d));assert(!live && !bus_owned);*failures[i]=0;
  assert(canvas_display_sticky_init(&d));d.release(&d);
 }
 /* Every SPI failure position must unwind power, including sleep failure. */
 assert(canvas_display_sticky_init(&d));unsigned begin=transfers;assert(d.present(&d));unsigned total=transfers-begin;
 for(unsigned i=1;i<=total;i++) {fail_spi=transfers+i;assert(!d.present(&d));assert(!pins[47]);}
 fail_spi=0;d.release(&d);assert(!live && !bus_owned);
 puts("PASS Sticky: 800x480 PSRAM, full dual-plane pixels/orientation, <=64-byte SPI, BUSY timeout, watchdog service, sleep/power-off, all SPI failure positions, init unwind and reopen");
}
