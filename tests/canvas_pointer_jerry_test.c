/* Real Canvas/events/Jerry/I2C ownership; only physical SDK/display are fakes. */
#include "jerryscript.h"
#include "canvas_display.h"
#include "pin_policy.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static i2c_inst_t buses[2]={{0},{1}};
i2c_inst_t *i2c0=&buses[0], *i2c1=&buses[1];
static unsigned inits, deinits, reads, writes, resets, delays;
static uint8_t reg, count, chip=0xb5;
static unsigned raw_x, raw_y;
static int read_fail=-99, write_fail=-99;
static bool bus_busy, fail_present, report_requires_stop, last_nostop;
static uint16_t pixels[240*280];
uint i2c_init(i2c_inst_t *i,uint baud) { assert(i==i2c1); inits++; return baud; }
void i2c_deinit(i2c_inst_t *i) { assert(i==i2c1); deinits++; }
uint32_t clock_get_hz(int clk) { (void)clk; return 150000000; }
void gpio_init(uint pin) { assert(pin<30); }
void gpio_set_function(uint pin,uint fn) { assert(pin<30); (void)fn; }
void gpio_pull_up(uint pin) { assert(pin==6 || pin==7); }
void gpio_set_dir(uint pin,bool out) { assert(pin==22 && out); }
void gpio_put(uint pin,bool high) { assert(pin==22); (void)high; resets++; }
void sleep_ms(uint32_t ms) { assert(ms==100); delays++; }
int i2c_write_timeout_us(i2c_inst_t *i,uint8_t a,const uint8_t *data,size_t n,bool nostop,uint timeout) {
 assert(i==i2c1 && a==0x15 && timeout==5000); writes++;
 if(write_fail!=-99) return write_fail;
 if(n==1) { last_nostop=nostop; reg=data[0]; assert(reg==0xa7 || reg==2); }
 else { assert(n==2 && !nostop); assert((data[0]==0xfe && data[1]==7) || (data[0]==0xfa && data[1]==0x60)); }
 return (int)n;
}
int i2c_read_timeout_us(i2c_inst_t *i,uint8_t a,uint8_t *data,size_t n,bool nostop,uint timeout) {
 assert(i==i2c1 && a==0x15 && !nostop && timeout==5000); reads++;
 if(read_fail!=-99) return read_fail;
 if(report_requires_stop && reg==2 && last_nostop) return -1;
 if(reg==0xa7) { assert(n==1); data[0]=chip; }
 else { assert(reg==2 && n==5); data[0]=count; data[1]=(raw_x>>8)|0x80; data[2]=raw_x; data[3]=raw_y>>8; data[4]=raw_y; }
 return (int)n;
}
static uint16_t *acquire(canvas_display_t *d) { (void)d; return pixels; }
static bool present(canvas_display_t *d) { (void)d; return !fail_present; }
static void release(canvas_display_t *d) { (void)d; bus_busy=false; }
bool canvas_display_st7789_init(canvas_display_t *d,const canvas_lcd_config_t *c) {
 if(bus_busy)return false;
 bus_busy=true; d->width=c->width; d->height=c->height;
 d->acquire=acquire; d->present=present; d->release=release; return true;
}
extern jerry_value_t js_create_canvas_module(void),js_create_canvas_native_module(void),js_create_events_module(void),js_create_i2c_module(void);
extern void js_canvas_present(void),js_canvas_reset(void);
static jerry_value_t native,events,canvas;
void js_set_function(jerry_value_t o,const char *n,jerry_external_handler_t h) {
 jerry_value_t f=jerry_function_external(h),r=jerry_object_set_sz(o,n,f); assert(!jerry_value_is_exception(r)); jerry_value_free(r); jerry_value_free(f);
}
static jerry_value_t require_test(const jerry_call_info_t *i,const jerry_value_t a[],jerry_length_t n) {
 (void)i; assert(n==1); char s[64]={0}; jerry_string_to_buffer(a[0],JERRY_ENCODING_UTF8,(jerry_char_t*)s,63);
 if(!strcmp(s,"events"))return jerry_value_copy(events);
 if(!strcmp(s,"mcujs:canvas-native"))return jerry_value_copy(native);
 if(!strcmp(s,"canvas"))return jerry_value_copy(canvas);
 return jerry_throw_sz(JERRY_ERROR_COMMON,"Unknown test module");
}
static void set(jerry_value_t o,const char *n,jerry_value_t v) { jerry_value_t r=jerry_object_set_sz(o,n,v); assert(!jerry_value_is_exception(r)); jerry_value_free(r); jerry_value_free(v); }
static void eval(const char *s) {
 jerry_value_t r=jerry_eval((const jerry_char_t*)s,strlen(s),0);
 if(jerry_value_is_exception(r)) { jerry_value_t e=jerry_exception_value(r,false),t=jerry_value_to_string(e); char msg[300]={0}; jerry_string_to_buffer(t,JERRY_ENCODING_UTF8,(jerry_char_t*)msg,299); fprintf(stderr,"%s\n%s\n",s,msg); abort(); }
 jerry_value_free(r);
}
int main(void) {
 jerry_init(JERRY_INIT_EMPTY); jerry_value_t g=jerry_current_realm();
 set(g,"require",jerry_function_external(require_test)); events=js_create_events_module(); native=js_create_canvas_native_module(); canvas=js_create_canvas_module();
 set(g,"Canvas",jerry_value_copy(canvas)); set(g,"I2C",js_create_i2c_module());
 eval("function check(v,m){if(!v)throw Error(m);} var tick; function setInterval(f,ms){check(ms===16,'period');tick=f;return 1;} function clearInterval(){tick=undefined;} var d=Canvas.connect('default');var seen=[];['pointerdown','pointermove','pointerup','pointercancel'].forEach(function(t){d.canvas.addEventListener(t,function(e){seen.push([e.type,e.offsetX,e.offsetY,e.pointerId]);});});d.startPointer();");
 assert(inits==1 && resets==2 && delays==2 && reads==1 && writes==3);
 count=1;raw_x=0;raw_y=0;eval("tick();");raw_x=239;raw_y=279;eval("tick();");count=0;eval("tick();check(seen.length===3 && seen[1][1]===239 && seen[1][2]===279 && seen[2][0]==='pointerup','portrait drag');");
 count=1;eval("tick();d.close();check(seen.length===5 && seen[4][0]==='pointercancel' && tick===undefined,'close cancellation');");assert(deinits==1);
 puts("PASS: native pointer portrait drag, finger-count release, close cancellation and bus release");
 eval("function openTouch(opts){d=Canvas.connect('default',opts);seen=[];['pointerdown','pointermove','pointerup','pointercancel','error'].forEach(function(t){d.canvas.addEventListener(t,function(e){seen.push([e.type,e.offsetX,e.offsetY]);});});d.startPointer();}");
 raw_x=0;raw_y=0;count=1;
 eval("openTouch({horizontal:true});tick();check(seen[0][1]===0 && seen[0][2]===239,'landscape origin');");
 raw_x=239;raw_y=279;
 eval("tick();check(seen[1][1]===279 && seen[1][2]===0,'landscape far corner');");
 read_fail=-1;
 eval("tick();check(seen.length===4 && seen[2][0]==='pointercancel' && seen[3][0]==='error' && tick===undefined,'read failure cancels, never up');d.close();");
 read_fail=-99;
 puts("PASS: landscape mapping and failed-read cancellation/error");
 eval("openTouch();tick();d.canvas.getContext('2d').fillRect(0,0,1,1);");
 fail_present=true;js_canvas_present();fail_present=false;
 eval("check(d.state==='error' && tick===undefined && seen[1][0]==='pointercancel','presentation failure cancellation');");
 puts("PASS: automatic presentation failure notifies lifecycle");
 eval("openTouch();tick();var resetBusy=false;d.canvas.addEventListener('pointercancel',function(){try{Canvas.connect('default');}catch(e){resetBusy=e.code==='EBUSY';}});");
 js_canvas_reset();
 eval("check(resetBusy && d.state==='closed' && tick===undefined && seen[1][0]==='pointercancel','orderly reset cancellation and reopen guard');");
 puts("PASS: orderly reset cancellation and reentrant open guard");
 set(g,"nativeCanvas",jerry_value_copy(native));
 unsigned before_gc=deinits;
 eval("var gcCalls=0;(function(){var b=nativeCanvas.open('default',{});b.setLifecycle(function(){gcCalls++;b.close();});b.startPointer();})();");
 jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
 eval("check(gcCalls===0,'GC must not invoke JS');");
 assert(deinits==before_gc+1 && !bus_busy);
 puts("PASS: cyclic lifecycle is collectible; native GC releases bus without JS");
 /* A finite failing chain detects an unbounded same-turn rescan without hanging. */
 eval("var reopens=0,chainDisplay;function chain(){chainDisplay=nativeCanvas.open('default',{});chainDisplay.setLifecycle(function(){reopens++;if(reopens<3)chain();});chainDisplay.draw([4,0,0,1,1],'fill',[0,0,0,1],1);}chain();");
 fail_present=true;js_canvas_present();fail_present=false;
 eval("check(reopens===1,'automatic presentation must bound reentrant opens per turn');chainDisplay.setLifecycle(function(){});chainDisplay.close();chainDisplay=null;");
 puts("PASS: automatic presentation bounds callbacks that reopen failing displays");
 /* Preserve foreign pin owners and unwind failed controller probes. */
 mcujs_rp2_pin_claim(22,MCUJS_RP2_PIN_OWNER_GPIO);
 eval("d=Canvas.connect('default');var busy=false;try{d.startPointer();}catch(e){busy=e.code==='EBUSY';}check(busy,'foreign GPIO conflict');d.close();");
 assert(mcujs_rp2_pin_owner(22)==MCUJS_RP2_PIN_OWNER_GPIO);
 mcujs_rp2_pin_release(22,MCUJS_RP2_PIN_OWNER_GPIO);
 chip=0;
 eval("d=Canvas.connect('default');var io=false;try{d.startPointer();}catch(e){io=e.code==='EIO';}check(io,'wrong controller identity');d.close();");
 assert(mcujs_rp2_pin_owner(6)==MCUJS_RP2_PIN_OWNER_NONE && mcujs_rp2_pin_owner(22)==MCUJS_RP2_PIN_OWNER_NONE);
 chip=0xb5;count=1;raw_x=1;raw_y=1;
 eval("openTouch();tick();var busy=false;try{I2C.init({bus:1,sda:6,scl:7,frequency:400000});}catch(e){busy=e.code==='EBUSY';}check(busy,'public I2C cannot steal touch bus');");
 count=2;
 eval("tick();check(seen[1][0]==='pointercancel' && seen[2][0]==='error' && tick===undefined,'invalid count cancels');d.close();");
 assert(mcujs_rp2_pin_owner(6)==MCUJS_RP2_PIN_OWNER_NONE);
 puts("PASS: foreign owner preserved, wrong identity unwinds, I2C takeover rejected, invalid contact count cancels");
 report_requires_stop=true;count=1;raw_x=42;raw_y=63;
 eval("d=Canvas.connect('default');seen=[];d.canvas.addEventListener('pointerdown',function(e){seen.push(e.type);});d.startPointer();tick();check(seen[0]==='pointerdown' && typeof tick==='function','STOP-separated touch report remains operational');d.close();");
 report_requires_stop=false;
 puts("PASS: STOP-separated report survives controller repeated-start NACK");
 count=0;
 eval("var stale=nativeCanvas.open('default',{});stale.close();var fresh=nativeCanvas.open('default',{});fresh.startPointer();stale=null;");
 unsigned before_stale_gc=deinits;
 jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
 assert(deinits==before_stale_gc);
 eval("check(fresh.samplePointer().length===3,'retired owner GC must not stop replacement touch');fresh.close();fresh=null;");
 puts("PASS: retired owner GC preserves replacement touch lease");
 js_canvas_reset();jerry_value_free(canvas);jerry_value_free(native);jerry_value_free(events);jerry_value_free(g);jerry_cleanup();
 return 0;
}
