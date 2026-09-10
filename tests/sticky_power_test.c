#include "canvas_epaper_stubs/fake_idf.h"
#include <assert.h>
#include <stdio.h>
static int pins[49], restarts, fail_input;
static int64_t now;
static uint64_t outputs, inputs;
int gpio_set_level(int p,int v){pins[p]=v;return 0;}
int gpio_get_level(int p){assert(p==4);return pins[p];}
int64_t esp_timer_get_time(void){return now;}
void esp_restart(void){restarts++;}
int gpio_config(const gpio_config_t *c){
 if(c->mode==GPIO_MODE_OUTPUT)outputs=c->pin_bit_mask;
 else {inputs=c->pin_bit_mask;assert(c->pull_up_en==1);if(fail_input)return -1;}
 return 0;
}
#include "../platform/esp32/main/board_power.c"
static void sample(int level,int64_t elapsed){pins[4]=level;now+=elapsed;mcujs_board_power_task();}
int main(void){
 for(int i=0;i<49;i++)pins[i]=-1;
 pins[4]=0;
 assert(mcujs_board_power_init());assert(pins[45]==1&&pins[46]==1);
 int off[]={10,38,41,42,47,48};for(unsigned i=0;i<sizeof(off)/sizeof(off[0]);i++)assert(pins[off[i]]==0);
 assert(outputs==((1ULL<<45)|(1ULL<<46)|(1ULL<<10)|(1ULL<<38)|(1ULL<<41)|(1ULL<<42)|(1ULL<<47)|(1ULL<<48)));
 assert(pins[39]==-1&&pins[19]==-1&&pins[20]==-1);
 sample(0,10000000);assert(restarts==0); /* held through boot */
 sample(1,0);sample(1,49999);sample(0,0);sample(0,4000000);
 assert(restarts==0); /* a brief release glitch must not arm */
 sample(1,0);sample(1,50000);sample(0,0);
 sample(0,2999999);assert(restarts==0);
 sample(1,0);sample(0,0); /* release cancels the old hold */
 sample(0,2999999);assert(restarts==0);
 sample(0,1);assert(restarts==1);
 assert(inputs==(1ULL<<4));
 sample(0,10000000);sample(1,0);sample(0,4000000);assert(restarts==1);
 assert(mcujs_board_power_init());sample(0,10000000);assert(restarts==1);
 sample(1,0);sample(1,50000);sample(0,0);sample(0,3000000);assert(restarts==2);
 fail_input=1;assert(!mcujs_board_power_init());
 sample(1,0);sample(1,50000);sample(0,0);sample(0,4000000);assert(restarts==2);
 puts("PASS Sticky restart: startup hold guard, stable release, short-press cancellation, 3s threshold, one-shot, reinit and GPIO failure");
 puts("PASS Sticky power: vendor hold/lock preserved; unused rails off; charger and microphone signal pins untouched");
}
