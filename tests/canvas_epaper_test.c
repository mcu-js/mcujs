#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int pins[49],live,bus_owned,fail_spi,stuck,busy_ticks;static unsigned usb_ticks;static int64_t now;
static unsigned command_count,updates,full_updates,partial_updates;static uint8_t last_command,control,ram[5000],old_ram[5000];static size_t ram_len,old_len;
static unsigned spi_attempts,busy_reads,wdt_ticks;
static int fail_sleep,fail_sleep_data,fail_ram,fail_old_ram,timeout_refresh;
void vTaskDelay(unsigned ms){now+=(int64_t)ms*1000;}
int64_t esp_timer_get_time(void){return now;}
int gpio_get_level(int p){busy_reads++;assert(p==8);if(stuck)return 1;if(busy_ticks){busy_ticks--;return 1;}return 0;}
static unsigned gpio_writes;
int gpio_set_level(int p,int v){gpio_writes++;pins[p]=v;return 0;}
int gpio_config(const gpio_config_t *c){assert(!(c->pin_bit_mask&((1ULL<<19)|(1ULL<<20))));return 0;}
int esp_task_wdt_status(void *p){(void)p;return 0;}int esp_task_wdt_reset(void){wdt_ticks++;return 0;}
void usb_cdc_puts(const char *s){
 assert(strstr(s,"timeout"));
}
void usb_cdc_task(void){usb_ticks++;}
int spi_bus_initialize(int host,const spi_bus_config_t *b,int dma){assert(host==1&&dma==1&&b->mosi_io_num==13&&b->sclk_io_num==12&&b->miso_io_num==-1);bus_owned++;return 0;}
int spi_bus_add_device(int h,const spi_device_interface_config_t *d,spi_device_handle_t *s){assert(h==1&&d->mode==0&&d->clock_speed_hz==4000000&&d->spics_io_num==-1);*s=(void*)1;return 0;}
int spi_bus_remove_device(spi_device_handle_t s){assert(s);return 0;}int spi_bus_free(int h){assert(h==1);bus_owned--;return 0;}
void *heap_caps_malloc(size_t n,int caps){
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
 assert(n==90000);
#elif defined(MCUJS_EXPERIMENTAL_EPAPER_PARTIAL)
 assert(n==85000);
#else
 assert(n==80000);
#endif
 assert(caps==3);live++;return malloc(n);
}
static void tracked_free(void *p){if(p)live--;free(p);}
int spi_device_polling_transmit(spi_device_handle_t s,spi_transaction_t *t){
 spi_attempts++;assert(s&&!pins[11]&&pins[6]==0);if(fail_spi)return -1;
 const uint8_t *p=t->tx_buffer;size_t n=t->length/8;
 if(fail_sleep && !pins[10] && *p==0x10)return -1;
 if(fail_sleep_data && pins[10] && last_command==0x10)return -1;
 if(fail_old_ram && pins[10] && last_command==0x26)return -1;
 if(fail_ram && pins[10] && last_command==0x24 && ram_len>=25)return -1;
 if(!pins[10]){assert(n==1);last_command=*p;command_count++;if(*p==0x24)ram_len=0;if(*p==0x26)old_len=0;if(*p==0x20){updates++;busy_ticks=2;if(control==0xc7)full_updates++;if(control==0xcf)partial_updates++;}}
 else if(last_command==0x24){assert(ram_len+n<=5000);memcpy(ram+ram_len,p,n);ram_len+=n;}
 else if(last_command==0x26){assert(old_len+n<=5000);memcpy(old_ram+old_len,p,n);old_len+=n;}
 else if(last_command==0x22){assert(n==1);control=*p;}
 else if(last_command==0x45){assert(n==4&&p[0]==199&&p[1]==0&&p[2]==0&&p[3]==0);}
 else if(last_command==0x10){assert(n==1&&p[0]==1);}
 if(timeout_refresh && !pins[10] && *p==0x20 && (control==0xc7||control==0xcf))stuck=1;
 return 0;
}
#define free tracked_free
#include "../platform/esp32/main/canvas_display_epaper154.c"
#undef free
#ifndef MCUJS_CANVAS_FIXTURE_ONLY
static bool present_ok(canvas_display_t *d){
 bool ok=d->present(d);assert(ok&&pins[6]==1);
 return ok;
}
/* Every flag combination exercises the same lifecycle and fault contract. */
static void test_lifecycle(void){
 canvas_display_t d={0};unsigned full=full_updates;
 assert(canvas_display_epaper154_init(&d));
 assert(d.width==200&&d.height==200&&pins[6]==1&&live==1);
 uint16_t *pixels=d.acquire(&d);
 assert(present_ok(&d));assert(full_updates==full+1);
 for(unsigned i=0;i<5000;i++)assert(ram[i]==0xff);
 /* RGB bit order at both ends, with independent Bayer/threshold literals. */
 pixels[0]=0xf800;pixels[1]=0x07e0;pixels[2]=0x001f;pixels[39999]=0;
 assert(present_ok(&d));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DITHER
 assert(ram[0]==0xdf);
#else
 assert(ram[0]==0x5f);
#endif
 assert(ram[4999]==0xfe);
 for(unsigned i=0;i<40000;i++)pixels[i]=0xffff;
 assert(present_ok(&d));
 for(unsigned fault=0;fault<7;fault++){
#ifndef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
  if(fault==5)continue; /* No second RAM plane in full-only builds. */
#endif
  for(unsigned i=0;i<40000;i++)pixels[i]=0;
  fail_spi=(fault==0);fail_ram=(fault==1);fail_sleep=(fault==2);
  timeout_refresh=(fault==3);fail_sleep_data=(fault==4);
  fail_old_ram=(fault==5);stuck=(fault==6);
  unsigned usb=usb_ticks,wdt=wdt_ticks;int64_t before=now;
  assert(!d.present(&d));assert(pins[6]==1);
  if(fault==3||fault==6){
   assert(now-before>=15000000&&now-before<16000000);
   assert(usb_ticks>usb&&wdt_ticks>wdt);
  }
  fail_spi=fail_ram=fail_sleep=timeout_refresh=fail_sleep_data=fail_old_ram=stuck=0;
  /* Retry the LAST successful image: invalid history must never suppress it. */
  for(unsigned i=0;i<40000;i++)pixels[i]=0xffff;
  full=full_updates;unsigned partial=partial_updates;
  assert(present_ok(&d));assert(full_updates==full+1&&partial_updates==partial);
  for(unsigned i=0;i<5000;i++)assert(ram[i]==0xff);
  unsigned commands=command_count;
  assert(present_ok(&d));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
  assert(command_count==commands); /* Only the recovered SUCCESS is cached. */
#else
  assert(command_count>commands);
#endif
 }
 d.release(&d);assert(!d.state&&live==0&&bus_owned==0&&pins[6]==1);
 full=full_updates;assert(canvas_display_epaper154_init(&d));
 assert(present_ok(&d));assert(full_updates==full+1);
 d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper lifecycle: first white, RGB/bit order, SPI/plane and sleep failures, bounded BUSY, USB/watchdog, full recovery, successful cache, close/reopen");
}
static void test_budget(void){
 canvas_display_t d={0};unsigned full=full_updates,partial=partial_updates;
 assert(canvas_display_epaper154_init(&d));assert(present_ok(&d));
 uint16_t *pixels=d.acquire(&d);
 for(unsigned i=0;i<4;i++){
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
  unsigned commands=command_count,refreshes=updates;
  assert(present_ok(&d));assert(command_count==commands&&updates==refreshes);
#endif
  pixels[0]=(i%2)?0xffff:0;assert(present_ok(&d));
 }
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(full_updates==full+1&&partial_updates==partial+4);
#else
 assert(full_updates==full+5&&partial_updates==partial);
#endif
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
 unsigned commands=command_count;
 assert(present_ok(&d));assert(command_count==commands); /* No cleaning on skip. */
#endif
 pixels[0]=0;assert(present_ok(&d));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(full_updates==full+2&&partial_updates==partial+4);
#else
 assert(full_updates==full+6&&partial_updates==partial);
#endif
 d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper budget: full first, four partials then full, skipped updates never clean or consume budget");
}
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DITHER
static void test_dither(void){
 canvas_display_t d={0};assert(canvas_display_epaper154_init(&d));
 uint16_t *pixels=d.acquire(&d);
 const uint16_t levels[]={0x0000,0x4208,0x8410,0xc618,0xffff};
 /* Independent literal 4x4 Bayer expectations: 0, 4, 8, 12, 16 white dots. */
 const uint8_t rows[5][4]={{0,0,0,0},{0xaa,0,0xaa,0},{0xaa,0x55,0xaa,0x55},{0xff,0x55,0xff,0x55},{0xff,0xff,0xff,0xff}};
 for(unsigned level=0;level<5;level++){
  for(unsigned i=0;i<40000;i++)pixels[i]=levels[level];
  assert(present_ok(&d));
  for(unsigned y=0;y<200;y++)for(unsigned x=0;x<25;x++)
   assert(ram[y*25+x]==rows[level][y%4] && "uniform gray must use screen-anchored Bayer dots");
  for(unsigned i=0;i<40000;i++)assert(pixels[i]==levels[level]);
 }
 /* All 17 achievable 4x4 coverage levels, independent expected dot count. */
 const uint16_t ramp[]={0,0x1082,0x2104,0x3186,0x4208,0x528a,0x630c,0x738e,0x8410,0x9492,0xa514,0xb596,0xc618,0xd69a,0xe71c,0xf79e,0xffff};
 for(unsigned level=0;level<17;level++){
  for(unsigned i=0;i<40000;i++)pixels[i]=ramp[level];
  assert(present_ok(&d));unsigned white=0;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)white+=!!(ram[y*25]&(0x80>>x));
  assert(white==level);
 }
 for(unsigned i=0;i<40000;i++)pixels[i]=0x8410;
 assert(present_ok(&d));uint8_t base[5000];memcpy(base,ram,5000);
 pixels[202]=0xffff; /* Local edit at (2,1); other Bayer dots must not move. */
 assert(present_ok(&d));base[25]=0x75;assert(!memcmp(base,ram,5000));
 pixels[202]=0x8410;pixels[203]=0xffff; /* Move to already-white Bayer site. */
 assert(present_ok(&d));base[25]=0x55;assert(!memcmp(base,ram,5000));
 unsigned commands=command_count;assert(present_ok(&d));assert(!memcmp(base,ram,5000));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
 assert(command_count==commands);
#else
 assert(command_count>commands);
#endif
 d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper DITHER: stable extrema, 17 uniform coverage levels, fixed screen-space pattern, localized edits, deterministic repeats, source unchanged");
}
#endif
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DIFF
static void expect_skip(canvas_display_t *d){
 unsigned commands=command_count,writes=gpio_writes,refreshes=updates,usb=usb_ticks;
 unsigned reads=busy_reads,watchdogs=wdt_ticks,transfers=spi_attempts;
 int64_t before=now;
 assert(d->present(d));
 assert(command_count==commands && "identical output must skip panel commands");
 assert(gpio_writes==writes&&updates==refreshes&&usb_ticks==usb&&now==before);
 assert(busy_reads==reads&&wdt_ticks==watchdogs&&spi_attempts==transfers);
}
int main(void){
 test_lifecycle();test_budget();
 command_count=updates=full_updates=partial_updates=0;
 canvas_display_t d={0};assert(canvas_display_epaper154_init(&d));
 canvas_display_t second={0};assert(!canvas_display_epaper154_init(&second));
 uint16_t *pixels=d.acquire(&d);assert(pixels[0]==0xffff&&pixels[39999]==0xffff);
 assert(command_count==0);assert(present_ok(&d));assert(full_updates==1);
 for(unsigned i=0;i<5000;i++)assert(ram[i]==0xff); /* Initial white must refresh. */
 expect_skip(&d);
 pixels[0]=0xfffe;expect_skip(&d); /* Changed RGB565, still monochrome white. */
 pixels[0]=0;assert(present_ok(&d));assert(ram[0]==0x7f&&ram[4999]==0xff);
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(full_updates==1&&partial_updates==1&&old_ram[0]==0xff);
#else
 assert(full_updates==2&&partial_updates==0);
#endif
 pixels[0]=1;expect_skip(&d); /* Dark blue remains black, even with dithering. */
 for(unsigned i=0;i<3;i++){
  for(unsigned j=0;j<7;j++)expect_skip(&d);
  pixels[0]=(i%2)?0:0xffff;assert(present_ok(&d));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
  assert(old_ram[0]==((i%2)?0xff:0x7f));
#endif
 }
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(partial_updates==4&&full_updates==1);
#else
 assert(partial_updates==0&&full_updates==5);
#endif
 expect_skip(&d); /* An unchanged frame at the cleaning boundary must not clean. */
 pixels[0]=0;assert(present_ok(&d));
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(full_updates==2&&partial_updates==4&&old_ram[0]==0x7f);
#else
 assert(full_updates==6&&partial_updates==0);
#endif
 for(unsigned fault=0;fault<4;fault++){
  uint16_t saved=pixels[0];pixels[0]=saved?0:0xffff;
  fail_spi=(fault==0);fail_ram=(fault==1);fail_sleep=(fault==2);timeout_refresh=(fault==3);
  int64_t before=now;assert(!d.present(&d));assert(pins[6]==1);
  if(fault==3)assert(now-before>=15000000&&now-before<16000000);
  fail_spi=fail_ram=fail_sleep=timeout_refresh=stuck=0;
  if(fault==0)pixels[0]=saved; /* Reverting to last successful image must NOT skip. */
  unsigned full=full_updates,partial=partial_updates;
  assert(present_ok(&d));assert(full_updates==full+1&&partial_updates==partial);
  expect_skip(&d);
 }
 unsigned full=full_updates;
 d.release(&d);assert(live==0&&bus_owned==0);
 assert(canvas_display_epaper154_init(&d));assert(present_ok(&d));assert(full_updates==full+1);
 expect_skip(&d);d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper DIFF: initial white, no-op/source-only skips, unchanged cleaning budget, transfer/sleep/BUSY failure invalidation, full recovery, close/reopen");
#ifdef MCUJS_EXPERIMENTAL_EPAPER_DITHER
 test_dither();
#endif
}
#elif defined(MCUJS_EXPERIMENTAL_EPAPER_DITHER)
int main(void){test_lifecycle();test_budget();test_dither();}
#else
int main(void){
 test_lifecycle();test_budget();
 command_count=updates=full_updates=partial_updates=0;
 canvas_display_t d={0};assert(canvas_display_epaper154_init(&d));assert(d.width==200&&d.height==200&&pins[6]==1&&command_count==0&&live==1);
 canvas_display_t second={0};assert(!canvas_display_epaper154_init(&second));
 uint16_t *p=d.acquire(&d);assert(p[0]==0xffff&&p[39999]==0xffff);p[0]=0xf800;p[1]=0x07e0;p[2]=0x001f;p[39999]=0;
 assert(present_ok(&d));assert(ram_len==5000&&ram[0]==0x5f&&ram[4999]==0xfe&&last_command==0x10&&pins[6]==1&&usb_ticks>0&&updates==2);
#ifdef MCUJS_EXPERIMENTAL_EPAPER_PARTIAL
 assert(old_len==5000 && old_ram[0]==0x5f && old_ram[4999]==0xfe);
 p[0]=0xffff;assert(present_ok(&d));
 assert(full_updates==1 && partial_updates==1 && old_ram[0]==0x5f && ram[0]==0xdf && pins[6]==1 && last_command==0x10);
 for(int i=0;i<3;i++){p[0]=(i%2)?0xffff:0;assert(present_ok(&d));}
 assert(partial_updates==4 && full_updates==1);
 assert(present_ok(&d));assert(full_updates==2 && partial_updates==4 && pins[6]==1);
 fail_spi=1;assert(!d.present(&d));assert(pins[6]==1);fail_spi=0;
 assert(present_ok(&d));assert(full_updates==3 && partial_updates==4);
#endif
 d.release(&d);assert(live==0&&bus_owned==0);
 assert(canvas_display_epaper154_init(&d));stuck=1;int64_t before=now;assert(!d.present(&d));assert(now-before>=15000000&&now-before<16000000&&pins[6]==1);d.release(&d);stuck=0;
 assert(canvas_display_epaper154_init(&d));fail_spi=1;assert(!d.present(&d));assert(pins[6]==1);d.release(&d);assert(live==0&&bus_owned==0);
 puts("PASS ePaper: no refresh on open, RGB565 threshold/bit order, full frame, BUSY wait/timeout, sleep/power-off, transfer failure, ownership and release");
}
#endif
#endif
