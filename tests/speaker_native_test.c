#include "bindings.h"
#include "speaker_hardware_stubs.h"
#include "speaker_test_fs.h"
pio_hw_t test_pio;
bool test_fail_sm,test_fail_program,test_fail_dma,test_fail_alarm,test_dma_busy,test_enabled;
unsigned test_sm_claims,test_program_claims,test_dma_claims,test_launches,test_pin_high;
uint64_t test_now;
alarm_id_t test_alarm;
alarm_callback_t test_callback;
extern jerry_value_t js_create_speaker_native_module(void);
extern void js_speaker_cleanup(void);
static void eval(const char *s){jerry_value_t v=jerry_eval((const jerry_char_t*)s,strlen(s),0);if(jerry_value_is_exception(v)){jerry_value_t e=jerry_exception_value(v,false),t=jerry_value_to_string(e);char b[300]={0};jerry_string_to_buffer(t,JERRY_ENCODING_UTF8,(jerry_char_t*)b,299);fprintf(stderr,"%s: %s\n",s,b);jerry_value_free(t);jerry_value_free(e);}assert(!jerry_value_is_exception(v));jerry_value_free(v);}
static void irq_after(uint64_t elapsed,bool complete){test_now+=elapsed;if(complete)test_dma_busy=false;assert(test_alarm&&test_callback);if(!test_callback(test_alarm,NULL))test_alarm=0;}
static void released(void){assert(!test_sm_claims&&!test_program_claims&&!test_dma_claims&&!test_enabled&&!test_alarm&&!handles);}
int main(void){
 jerry_init(JERRY_INIT_EMPTY);jerry_value_t g=jerry_current_realm(),n=js_create_speaker_native_module(),v=jerry_object_set_sz(g,"N",n);jerry_value_free(v);jerry_value_free(n);jerry_value_free(g);
 eval("function eq(a,b){if(a!==b)throw Error(a+' != '+b);}function fails(f,c){try{f();}catch(e){eq(e.code,c);return;}throw Error('expected '+c);}var h=N.open();fails(function(){N.open();},'EBUSY');");
 wav_fixture();
 bool *flags[]={&test_fail_sm,&test_fail_program,&test_fail_dma,&test_fail_alarm};
 for(unsigned i=0;i<4;i++){*flags[i]=true;eval("fails(function(){N.start(h,'/app/chime.wav',0.25);},'ERR_RESOURCE_EXHAUSTED');");released();*flags[i]=false;}
 access=FS_ERROR_BUSY;eval("fails(function(){N.start(h,'/app/chime.wav',0.25);},'EBUSY');");access=FS_OK;released();
 // Normal stream: refill through the actual public native module; no full-file buffer.
 wav_fixture();eval("N.start(h,'/app/chime.wav',0.25);eq(N.poll(h),1);fails(function(){N.start(h,'/app/chime.wav',0.25);},'EBUSY');");
 for(unsigned i=0;i<8;i++){irq_after(32000,true);eval("eq(N.poll(h),1);");}
 assert(test_enabled);irq_after(1000,false);assert(!test_enabled);eval("eq(N.poll(h),2);N.stop(h);");released();assert(max_read<=1024);
 // IRQ progresses two prepared buffers then silences with no JS service.
 wav_fixture();eval("N.start(h,'/app/chime.wav',0.25);");irq_after(32000,true);irq_after(32000,true);irq_after(1000,false);assert(!test_enabled&&!test_pin_high);
 eval("fails(function(){N.poll(h);},'EIO');N.close(h);var h=N.open();");released();
 // Stop/close and a new token; cleanup is safe during engine teardown.
 for(unsigned i=0;i<20;i++){wav_fixture();eval("N.start(h,'/app/chime.wav',0.25);N.stop(h);");released();}
 eval("var old=h;N.close(h);h=N.open();fails(function(){N.start(old,'/app/chime.wav',0.25);},'ENXIO');");
 // A delayed DMA handoff must not silently resume after inserting zeros.
 wav_fixture();eval("N.start(h,'/app/chime.wav',0.25);");unsigned launches=test_launches;irq_after(34000,true);assert(test_launches==launches);irq_after(1000,false);assert(!test_enabled);eval("fails(function(){N.poll(h);},'EIO');");released();
 wav_fixture();eval("N.start(h,'/app/chime.wav',0.25);");js_speaker_cleanup();released();eval("fails(function(){N.poll(h);},'ENXIO');h=N.open();N.close(h);");
 jerry_cleanup();puts("PASS native speaker: bounded streaming, drain, IRQ starvation/late handoff silence, resource failure rollback, stop/repeat, stale tokens and teardown");
}
