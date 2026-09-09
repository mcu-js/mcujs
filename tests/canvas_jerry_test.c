/* Real JerryScript + production Canvas bindings/ctx, with a host display surface. */
#include "jerryscript.h"
#include "mcujs_dvi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t a[160*120],b[160*120],*display=a,*back=b;
static mcujs_dvi_state_t state;
static unsigned presents;
bool mcujs_dvi_init(uint16_t w,uint16_t h){state.width=w;state.height=h;state.initialized=true;return true;}
bool mcujs_dvi_start(void){state.running=true;memset(a,0,sizeof(a));memset(b,0,sizeof(b));return true;}
bool mcujs_dvi_is_running(void){return state.running;}
const mcujs_dvi_state_t *mcujs_dvi_get_state(void){return &state;}
uint16_t *mcujs_dvi_get_draw_buffer(void){return back;}
bool mcujs_dvi_swap_and_show(void){uint16_t *p=display;display=back;back=p;presents++;return true;}
extern jerry_value_t js_create_canvas_native_module(void);
extern jerry_value_t js_create_canvas_module(void);
extern void js_canvas_present(void);
static jerry_value_t require_native(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc){(void)info;(void)args;assert(argc==1);return js_create_canvas_native_module();}
static void evaluate(const char *text,size_t size){
 jerry_value_t result=jerry_eval((const jerry_char_t*)text,size,JERRY_PARSE_NO_OPTS);
 if(jerry_value_is_exception(result)){
  jerry_value_t error=jerry_exception_value(result,false),string=jerry_value_to_string(error);
  char message[512]={0};jerry_size_t n=jerry_string_to_buffer(string,JERRY_ENCODING_UTF8,(jerry_char_t*)message,sizeof(message)-1);
  fprintf(stderr,"JerryScript error: %.*s\n",(int)n,message);exit(1);
 }
 jerry_value_free(result);
}
static void source(const char *path){FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *s=malloc(n+1);assert(s);assert(fread(s,1,n,f)==(size_t)n);s[n]=0;fclose(f);evaluate(s,n);free(s);}
int main(int argc,char **argv){
 assert(argc==4);jerry_init(JERRY_INIT_EMPTY);
 jerry_value_t global=jerry_current_realm(),key=jerry_string_sz("require"),fn=jerry_function_external(require_native);
 jerry_value_t result=jerry_object_set(global,key,fn);jerry_value_free(result);jerry_value_free(fn);jerry_value_free(key);jerry_value_free(global);
 const char *preamble="var module={exports:{}};";evaluate(preamble,strlen(preamble));
 jerry_value_t packaged=js_create_canvas_module();assert(!jerry_value_is_exception(packaged));
 global=jerry_current_realm();key=jerry_string_sz("packagedCanvas");result=jerry_object_set(global,key,packaged);
 jerry_value_free(result);jerry_value_free(key);jerry_value_free(global);jerry_value_free(packaged);
 const char *save="var canvasUnderTest=packagedCanvas.canvas;";evaluate(save,strlen(save));
 source(argv[2]);const char *run="module.exports(canvasUnderTest);";evaluate(run,strlen(run));
 assert(presents==0);js_canvas_present();assert(presents==1);
 js_canvas_present();assert(presents==1); /* no pending work */
 FILE *f=fopen(argv[3],"wb");assert(f);fprintf(f,"P6\n160 120\n255\n");
 for(int i=0;i<160*120;i++){uint16_t p=display[i];unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
 printf("Real JerryScript Canvas demo: %u complete frame presentations, saved host RGB565 image\n",presents);
 jerry_cleanup();return 0;
}
