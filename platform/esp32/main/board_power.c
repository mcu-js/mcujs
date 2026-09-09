/* Private V2 power latch, from vendor 07_BATT_PWR_Test/user_config.h.
 * GPIO17 high holds battery power; GPIO18 is active-low PWR; GPIO42 high
 * disables the unimplemented audio rail. Not a public GPIO/PMIC API. */
#include "board_power.h"
#ifdef MCUJS_BOARD_SEEED_RETERMINAL_STICKY
#include "driver/gpio.h"
bool mcujs_board_power_init(void) {
 /* Seeed Sticky_dashboard_demo board.cpp: hold and lock high.
  * Unused SD/touch/microphone/buzzer rails stay off; charger is untouched. */
 const int pins[]={45,46,10,38,41,42,47,48};
 uint64_t mask=0;
 for(unsigned i=0;i<sizeof(pins)/sizeof(pins[0]);i++) {
  int level=i<2?1:0;
  if(gpio_set_level(pins[i],level)!=0)return false;
#ifdef ESP_PLATFORM
  (void)gpio_hold_dis(pins[i]);
#endif
  mask|=1ULL<<pins[i];
 }
 gpio_config_t outputs={.pin_bit_mask=mask,.mode=GPIO_MODE_OUTPUT};
 return gpio_config(&outputs)==0;
}
void mcujs_board_power_task(void) {}
#elif defined(MCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2)
#include "driver/gpio.h"
#include "esp_timer.h"
static bool armed,enabled;
static int64_t pressed_at;
bool mcujs_board_power_init(void) {
 armed=false;enabled=false;pressed_at=-1;
 /* Preload output levels before enabling outputs. */
 if(gpio_set_level(17,1)!=0 || gpio_set_level(42,1)!=0)return false;
 gpio_config_t out={.pin_bit_mask=(1ULL<<17)|(1ULL<<42),.mode=GPIO_MODE_OUTPUT};
 gpio_config_t in={.pin_bit_mask=1ULL<<18,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 if(gpio_config(&out)!=0 || gpio_config(&in)!=0)return false;
 enabled=true;return true;
}
void mcujs_board_power_task(void) {
 if(!enabled)return;
 if(gpio_get_level(18)){armed=true;pressed_at=-1;return;}
 if(!armed)return; /* First release the button used to switch the board on. */
 int64_t now=esp_timer_get_time();
 if(pressed_at<0)pressed_at=now;
 if(now-pressed_at>=2000000){gpio_set_level(17,0);enabled=false;}
}
#else
bool mcujs_board_power_init(void){return true;}
void mcujs_board_power_task(void){}
#endif
