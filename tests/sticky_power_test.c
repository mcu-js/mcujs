#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <stdio.h>
static int pins[49];static uint64_t mask;
int gpio_set_level(int p,int v){pins[p]=v;return 0;}
int gpio_config(const gpio_config_t *c){mask|=c->pin_bit_mask;return 0;}
#include "../platform/esp32/main/board_power.c"
int main(void){for(int i=0;i<49;i++)pins[i]=-1;assert(mcujs_board_power_init());assert(pins[45]==1&&pins[46]==1);
 int off[]={10,38,41,42,47,48};for(unsigned i=0;i<sizeof(off)/sizeof(off[0]);i++)assert(pins[off[i]]==0);
 assert(mask==((1ULL<<45)|(1ULL<<46)|(1ULL<<10)|(1ULL<<38)|(1ULL<<41)|(1ULL<<42)|(1ULL<<47)|(1ULL<<48)));
 assert(pins[39]==-1&&pins[19]==-1&&pins[20]==-1);mcujs_board_power_task();puts("PASS Sticky power: vendor hold/lock and unused rails off; charger and microphone signal pins untouched");}
