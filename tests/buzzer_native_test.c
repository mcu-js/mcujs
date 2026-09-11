#include "bindings.h"
#include "runtime_validation_backend_stubs.h"
#include "pico/time.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern jerry_value_t js_create_buzzer_native_module(void);
extern void js_buzzer_cleanup(void);
static alarm_callback_t callback;
static alarm_id_t alarm, next_alarm;
static bool fail_alarm;
alarm_id_t add_alarm_in_ms(uint32_t ms, alarm_callback_t cb, void *data, bool past) {
 (void)data;(void)past;assert(ms>0 && ms<=1000);if(fail_alarm)return -1;callback=cb;return alarm=++next_alarm;
}
bool cancel_alarm(alarm_id_t id) {assert(id==alarm);alarm=0;return true;}
static void eval(const char *s) {
 jerry_value_t v=jerry_eval((const jerry_char_t*)s,strlen(s),0);
 if(jerry_value_is_exception(v)){jerry_value_t e=jerry_exception_value(v,false),t=jerry_value_to_string(e);char b[300]={0};jerry_string_to_buffer(t,JERRY_ENCODING_UTF8,(jerry_char_t*)b,299);fprintf(stderr,"%s\n",b);jerry_value_free(t);jerry_value_free(e);}
 assert(!jerry_value_is_exception(v));jerry_value_free(v);
}
static void install(const char *name,jerry_value_t value){jerry_value_t g=jerry_current_realm(),v=jerry_object_set_sz(g,name,value);jerry_value_free(v);jerry_value_free(g);jerry_value_free(value);}
int main(void){
 mcujs_test_reset_backend();jerry_init(JERRY_INIT_EMPTY);
 install("B",js_create_buzzer_native_module());install("PWM",js_create_pwm_module());install("GPIO",js_create_gpio_module());
 eval("function eq(a,b){if(a!==b)throw Error(a+' != '+b);} function fails(f,c){try{f();}catch(e){eq(e.code,c);return;}throw Error('expected '+c);}");
 eval("try{GPIO.init(2,GPIO.OUTPUT);throw Error('reserved pin accepted');}catch(e){if(e.message==='reserved pin accepted')throw e;} PWM.init(18,1000);fails(function(){B.open();},'EBUSY');PWM.stop(18);var h=B.open();fails(function(){PWM.init(18,1000);},'EBUSY');eq(B.start(h,1000,100),1000);eq(B.playing(h),true);");
 assert(alarm>0);unsigned disables=mcujs_test_pwm_disable_calls;
 callback(alarm,NULL);alarm=0; // hardware IRQ, deliberately no JS timer processing
 assert(mcujs_test_pwm_disable_calls==disables+1);assert(mcujs_test_gpio_level_at(2)==0);assert(mcujs_test_gpio_is_output(2));
 eval("eq(B.playing(h),false);B.start(h,1000,10);B.stop(h);eq(B.playing(h),false);");assert(!alarm);
 fail_alarm=true;eval("fails(function(){B.start(h,1000,10);},'ERR_RESOURCE_EXHAUSTED');eq(B.playing(h),false);");fail_alarm=false;
 eval("fails(function(){B.start(h,880,10);},'ERR_NOT_SUPPORTED');B.start(h,1000,10);");
 js_timers_cleanup();assert(!alarm);assert(mcujs_test_gpio_level_at(2)==0);
 eval("var n=B.open();fails(function(){B.start(h,1000,10);},'ENXIO');B.start(n,1000,10);B.close(n);PWM.init(18,1000);PWM.stop(18);");
 jerry_cleanup();puts("PASS native buzzer: IRQ shutdown without JS, alarm exhaustion, pin/slice exclusion, stop, timer teardown, stale handles and reopen");return 0;
}
