#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <stdio.h>
static int pins[49];static int64_t now;static uint64_t outputs,inputs;
int gpio_set_level(int p,int v){pins[p]=v;return 0;}
int gpio_get_level(int p){assert(p==18);return pins[p];}
int gpio_config(const gpio_config_t *c){if(c->mode==GPIO_MODE_OUTPUT)outputs=c->pin_bit_mask;else {inputs=c->pin_bit_mask;assert(c->pull_up_en==1);}return 0;}
int64_t esp_timer_get_time(void){return now;}
#include "../platform/esp32/main/board_power.c"
int main(void){
 pins[18]=0;assert(mcujs_board_power_init());assert(pins[17]==1&&pins[42]==1);
 assert(outputs==((1ULL<<17)|(1ULL<<42))&&inputs==(1ULL<<18));
 now=5000000;mcujs_board_power_task();assert(pins[17]==1); /* held through boot is not shutdown */
 pins[18]=1;mcujs_board_power_task();pins[18]=0;mcujs_board_power_task();
 now+=1000000;mcujs_board_power_task();assert(pins[17]==1);pins[18]=1;mcujs_board_power_task();
 pins[18]=0;mcujs_board_power_task();now+=1999999;mcujs_board_power_task();assert(pins[17]==1);
 now++;mcujs_board_power_task();assert(pins[17]==0&&pins[42]==1);
 pins[18]=1;mcujs_board_power_task();assert(pins[17]==0); /* USB cannot accidentally re-latch battery */
 assert(mcujs_board_power_init());assert(pins[17]==1);
 puts("PASS battery latch, audio-off, startup hold guard, short-press cancel, 2s shutdown and reinitialization");
}
