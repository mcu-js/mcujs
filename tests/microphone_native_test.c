#include "jerryscript.h"
#include "microphone_native_sdk.h"
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern jerry_value_t js_create_microphone_native_module(void);
extern void js_microphone_cleanup(void);
static void eval(const char *s) {
    jerry_value_t v=jerry_eval((const jerry_char_t *)s,strlen(s),0);
    if(jerry_value_is_exception(v)) {
        jerry_value_t e=jerry_exception_value(v,false),t=jerry_value_to_string(e);
        char b[512]={0};jerry_string_to_buffer(t,JERRY_ENCODING_UTF8,(jerry_char_t *)b,511);
        fprintf(stderr,"%s\n%s\n",s,b);jerry_value_free(t);jerry_value_free(e);
    }
    assert(!jerry_value_is_exception(v));jerry_value_free(v);
}
int main(void) {
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t g=jerry_current_realm(),n=js_create_microphone_native_module();
    jerry_value_free(jerry_object_set_sz(g,"N",n));jerry_value_free(n);jerry_value_free(g);
    eval("function eq(a,b){if(a!==b)throw Error(a+' != '+b);}function fails(f,c){try{f();}catch(e){eq(e.code,c);return;}throw Error('expected '+c);}eq(N.state(),'idle');var h=N.open();eq(N.state(),'busy');fails(function(){N.open();},'EBUSY');N.close(h);eq(N.state(),'idle');fails(function(){N.stop(h);},'ENXIO');");
    assert(!atomic_load(&fake_unmutes)&&!atomic_load(&fake_reads)&&!atomic_load(&fake_tasks));
    eval("h=N.open();[undefined,null,'20',19,1001,20.5,NaN,Infinity].forEach(function(x){fails(function(){N.start(h,x);},'ERR_OUT_OF_RANGE');});fails(function(){N.start(''+h,20);},'ENXIO');fails(function(){N.poll(h);},'EINVAL');");
    assert(!atomic_load(&fake_unmutes)&&!atomic_load(&fake_reads)&&!atomic_load(&fake_tasks));
    fake_reset();atomic_store(&fake_fail_alloc,true);
    eval("fails(function(){N.start(h,20);},'ERR_RESOURCE_EXHAUSTED');");fake_wait_idle();
    atomic_store(&fake_fail_task,true);
    eval("fails(function(){N.start(h,20);},'ERR_RESOURCE_EXHAUSTED');");fake_wait_idle();
    fake_reset();eval("N.start(h,20);fails(function(){N.start(h,20);},'EBUSY');");
    fake_wait_idle();
    int setup_steps=atomic_load(&fake_steps)-1; /* omit final force-mute */
    assert(atomic_load(&fake_unmutes)==1&&atomic_load(&fake_reads)>0);
    eval("eq(N.poll(h),2);var a=N.result(h);eq(a instanceof Uint8Array,true);eq(a.length,640);for(var i=0;i<a.length;i+=2){eq(a[i],46);eq(a[i+1],251);}fails(function(){N.result(h);},'EINVAL');N.stop(h);");
    /* Every admitted setup SDK operation, including ambiguous unmute failure. */
    for(int step=1;step<=setup_steps;step++) {
        fake_reset();atomic_store(&fake_fault_at,step);eval("N.start(h,20);");fake_wait_idle();
        eval("try{N.poll(h);throw Error('poll unexpectedly succeeded');}catch(e){if(e.code!=='EIO'&&e.code!=='ERR_RESOURCE_EXHAUSTED')throw e;}N.stop(h);");
    }
    fake_reset();eval("N.start(h,1000);");fake_wait_idle();
    eval("eq(N.poll(h),2);var a=N.result(h);eq(a.length,32000);N.stop(h);");
    /* JS deliberately never polls while worker/guard terminate missing input. */
    for(int mode=1;mode<=4;mode++) {
        fake_reset();atomic_store(&fake_read_mode,mode);eval("N.start(h,20);");fake_wait_idle();
        assert(atomic_load(&fake_reads)>0);
        if(mode==2||mode==3) assert(atomic_load(&fake_reads)==1);
        eval("fails(function(){N.poll(h);},'EIO');N.stop(h);");
    }
    /* A blocked worker cannot hold the rail: independent timer callback closes. */
    fake_reset();atomic_store(&fake_hold_read,true);eval("N.start(h,1000);");fake_wait_idle();
    long long elapsed=atomic_load(&fake_off_at)-atomic_load(&fake_unmuted_at);
    assert(elapsed>=900000&&elapsed<=1030000);
    eval("eq(N.poll(h),2);var a=N.result(h);if(a.length<2||a.length>32000)throw Error('bound');N.stop(h);");
    /* Stop/close while SDK read owns its resources: power must precede join. */
    for(int n=0;n<20;n++) {
        fake_reset();atomic_store(&fake_hold_read,true);eval("N.start(h,1000);");
        int64_t limit=esp_timer_get_time()+500000;
        while(!atomic_load(&fake_in_read)){assert(esp_timer_get_time()<limit);usleep(1000);}
        int64_t before=esp_timer_get_time();
        eval(n%2?"N.stop(h);":"var old=h;N.close(h);h=N.open();fails(function(){N.poll(old);},'ENXIO');");
        assert(atomic_load(&fake_off_at)-before<30000);fake_wait_idle();
    }
    /* Force-mute failure cannot bypass the checked rail boundary. */
    fake_reset();atomic_store(&fake_fail_mute,true);eval("N.start(h,20);");fake_wait_idle();
    eval("eq(N.poll(h),2);N.result(h);N.close(h);h=N.open();");
    fake_reset();atomic_store(&fake_hold_read,true);eval("N.start(h,1000);");
    while(!atomic_load(&fake_in_read)) usleep(1000);
    js_microphone_cleanup();fake_wait_idle();
    eval("fails(function(){N.poll(h);},'ENXIO');fails(function(){N.open();},'ENXIO');");
    /* Final irreversible privacy-fault latch rejects all future admissions. */
    n=js_create_microphone_native_module();jerry_value_free(n);eval("h=N.open();");
    atomic_store(&fake_fail_off,true);
    eval("fails(function(){N.start(h,20);},'ERR_MICROPHONE_PRIVACY');eq(N.state(),'busy');");
    atomic_store(&fake_fail_off,false);
    eval("fails(function(){N.close(h);},'ERR_MICROPHONE_PRIVACY');fails(function(){N.open();},'ERR_MICROPHONE_PRIVACY');");
    js_microphone_cleanup();fake_wait_idle();jerry_cleanup();
    printf("PASS microphone native: owner/validation, PCM bounds+LE, %d setup fault points, allocation/task failure, missing input, independent rail deadline, 20 stop/close races, mute fallback, cleanup, stale tokens and privacy latch\n",setup_steps);
}
