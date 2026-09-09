/* Real Jerry + shared rasterizer; fake physical displays expose output buffers. */
#include "jerryscript.h"
#include "canvas_display.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t buffers[2][320*320];
static bool occupied[2];
static unsigned opened,released,presented;
static canvas_lcd_config_t last_config;
static jerry_value_t native, api;
extern jerry_value_t js_create_canvas_native_module(void);
extern jerry_value_t js_create_canvas_module(void);
extern jerry_value_t js_create_st7789_module(void);
extern void js_canvas_present(void);
static uint16_t *acquire(canvas_display_t *d){return d->state;}
static bool present(canvas_display_t *d){(void)d;presented++;return true;}
static void release(canvas_display_t *d){for(int i=0;i<2;i++)if(d->state==buffers[i])occupied[i]=false;released++;d->state=NULL;}
bool canvas_display_st7789_init(canvas_display_t *d,const canvas_lcd_config_t *c){
 last_config=*c;
 int i;for(i=0;i<2;i++)if(!occupied[i])break;if(i==2)return false;
 occupied[i]=true;opened++;memset(buffers[i],0,sizeof(buffers[i]));
 d->width=c->width;d->height=c->height;d->state=buffers[i];d->acquire=acquire;d->present=present;d->release=release;return true;
}
static jerry_value_t require_stub(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc){
 (void)info;assert(argc==1);char name[64]={0};jerry_string_to_buffer(args[0],JERRY_ENCODING_UTF8,(jerry_char_t*)name,sizeof(name)-1);
 if(strcmp(name,"mcujs:canvas-native")==0)return jerry_value_copy(native);
 if(strcmp(name,"canvas")==0)return jerry_value_copy(api);
 return jerry_throw_sz(JERRY_ERROR_COMMON,"Unknown test module");
}
static void set(jerry_value_t object,const char *name,jerry_value_t value){
 jerry_value_t key=jerry_string_sz(name),r=jerry_object_set(object,key,value);assert(!jerry_value_is_exception(r));jerry_value_free(r);jerry_value_free(key);jerry_value_free(value);
}
static void eval(const char *code){
 jerry_value_t r=jerry_eval((const jerry_char_t*)code,strlen(code),JERRY_PARSE_NO_OPTS);
 if(jerry_value_is_exception(r)){
  jerry_value_t e=jerry_exception_value(r,false),text=jerry_value_to_string(e);char msg[300]={0};jerry_string_to_buffer(text,JERRY_ENCODING_UTF8,(jerry_char_t*)msg,299);fprintf(stderr,"%s\n%s\n",code,msg);abort();
 }
 jerry_value_free(r);
}
int main(void){
 jerry_init(JERRY_INIT_EMPTY);native=js_create_canvas_native_module();jerry_value_t g=jerry_current_realm();
 set(g,"require",jerry_function_external(require_stub));api=js_create_canvas_module();assert(!jerry_value_is_exception(api));assert(opened==0);
 set(g,"Canvas",jerry_value_copy(api));set(g,"ST7789",js_create_st7789_module());assert(opened==0);
#ifdef MCUJS_CANVAS_DEFAULT_ST7789_1_69
 eval("var d=Canvas.display;if(d.canvas.width!==240||d.canvas.height!==280)throw Error('wrong V2 portrait default');var c=d.canvas.getContext('2d');c.fillStyle='red';c.fillRect(0,0,240,280);");
 assert(last_config.profile==CANVAS_PANEL_WAVESHARE_1_69 && last_config.spi==1 && last_config.sck==10 && last_config.mosi==11 && last_config.cs==9 && last_config.dc==8 && last_config.reset==13 && last_config.backlight==25 && last_config.x_offset==0 && last_config.y_offset==20 && !last_config.horizontal);
 js_canvas_present();assert(buffers[0][67199]==0xf800);
 eval("d.close();var e=ST7789.connect();if(e.canvas.height!==280)throw Error('no-option V2');e.close();var f=ST7789.connect({});if(f.canvas.width!==240)throw Error('empty V2');f.close();var h=ST7789.connect({profile:'waveshare-1.69',horizontal:true});if(h.canvas.width!==280||h.canvas.height!==240)throw Error('V2 landscape');h.close();");
 assert(last_config.x_offset==20 && last_config.y_offset==0 && last_config.horizontal);
 eval("var no=false;try{ST7789.connect({profile:'waveshare-1.69',width:320});}catch(e){no=true;}if(!no)throw Error('V2 invalid geometry');var old=ST7789.connect({profile:'waveshare-1.47'});if(old.canvas.width!==320||old.canvas.height!==172)throw Error('old defaults changed');old.close();");
 assert(opened==5 && released==5);
 jerry_value_free(g);jerry_value_free(api);jerry_value_free(native);jerry_cleanup();
 puts("PASS: real Jerry V2 board defaults, portrait pixels, explicit landscape offsets, geometry rejection, old profile defaults and close/reopen");return 0;
#endif
#ifdef MCUJS_CANVAS_DEFAULT_ST7789_2_8
 eval("var d=Canvas.display;if(d.canvas.width!==320||d.canvas.height!==240)throw Error('wrong default T3 geometry');var c=d.canvas.getContext('2d');c.fillStyle='red';c.fillRect(0,0,320,240);");
 assert(last_config.profile==CANVAS_PANEL_WAVESHARE_2_8 && last_config.spi==1 && last_config.sck==10 && last_config.mosi==11 && last_config.cs==13 && last_config.dc==14 && last_config.reset==15 && last_config.backlight==16 && last_config.x_offset==0 && last_config.y_offset==0);
 js_canvas_present();assert(buffers[0][76799]==0xf800);
 eval("d.close();var d2=ST7789.connect();if(d2.canvas.height!==240)throw Error('wrong no-option T3 geometry');d2.close();var d3=ST7789.connect({});if(d3.canvas.height!==240)throw Error('wrong empty-option T3 geometry');d3.close();");
 assert(opened==3 && released==3);
 jerry_value_free(g);jerry_value_free(api);jerry_value_free(native);jerry_cleanup();
 puts("PASS: real Jerry T3 board defaults, lazy canvas, full 320x240 pixels, empty/absent options and close/reopen");return 0;
#endif

 eval("var a=ST7789.connect();var b=ST7789.connect({cs:9,dc:8,reset:13,backlight:25,horizontal:false});var ac=a.canvas.getContext('2d');var bc=b.canvas.getContext('2d');if(a.canvas===b.canvas||a.canvas.width!==320||b.canvas.width!==172)throw Error('identity/dimensions');ac.fillStyle='#ff0000';ac.fillRect(0,0,3,3);bc.fillStyle='#0000ff';bc.fillRect(0,0,3,3);");
 assert(opened==2 && presented==0);js_canvas_present();assert(presented==2);
 assert(buffers[0][0]==0xf800 && buffers[1][0]==0x001f);
 eval("ac.fillStyle='#ffffff';ac.fillRect(1,1,1,1);");js_canvas_present();
 assert(buffers[0][0]==0xf800 && buffers[0][321]==0xffff && buffers[1][0]==0x001f);
 eval("a.close();a.close();var rejected=false;try{ac.fillRect(0,0,1,1);}catch(e){rejected=true;}if(!rejected)throw Error('closed target accepted draw');bc.fillStyle='#00ff00';bc.fillRect(0,0,1,1);");
 assert(released==1);js_canvas_present();assert(buffers[1][0]==0x07e0 && presented==4);
 eval("var rejected=false;try{ST7789.connect({i2c:0});}catch(e){rejected=true;}if(!rejected)throw Error('unsupported transport ignored');");assert(opened==2);
 eval("var rejected=false;try{ST7789.connect({sck:19});}catch(e){rejected=true;}if(!rejected)throw Error('invalid SPI route accepted');");assert(opened==2);
 eval("var rejected=false;try{ST7789.connect({width:240});}catch(e){rejected=true;}if(!rejected)throw Error('unqualified geometry accepted');");assert(opened==2);
 eval("b.close();");assert(released==2);
 eval("var d=Canvas.display;if(d.canvas!==Canvas.canvas||Canvas.display!==d)throw Error('default identity');d.canvas.getContext('2d').fillRect(0,0,1,1);d.close();");assert(opened==3 && released==3);
 js_canvas_present();assert(presented==4);
 eval("var p28=ST7789.connect({profile:'waveshare-2.8',spi:1,sck:10,mosi:11,cs:13,dc:14,reset:15,backlight:16});if(p28.canvas.width!==320||p28.canvas.height!==240)throw Error('2.8 dimensions');var t=p28.canvas.getContext('2d');t.fillStyle='lime';t.fillRect(0,0,320,240);");
 assert(last_config.profile==CANVAS_PANEL_WAVESHARE_2_8 && last_config.spi==1 && last_config.x_offset==0 && last_config.y_offset==0);
 js_canvas_present();assert(buffers[0][0]==0x07e0 && buffers[0][76799]==0x07e0);
 eval("p28.close();var no=false;try{ST7789.connect({profile:'unknown'});}catch(e){no=true;}if(!no)throw Error('unknown profile accepted');");
 assert(opened==4 && released==4);
 jerry_value_free(g);jerry_value_free(api);jerry_value_free(native);jerry_cleanup();
 assert(released==4);puts("PASS: real Jerry per-display RGB565 pixels, isolation, retained drawing, close, default identity, SPI option rejection");return 0;
}
