/* Private experimental Canvas backend. Applications use lib/canvas.js. */
#include "jerryscript.h"
#include "canvas_renderer.h"
#include "mcujs_dvi.h"
#include <math.h>
#include <string.h>
#define WIDTH 160
#define HEIGHT 120
#define MAX_COMMANDS 128
static const uint16_t *last_presented;

static bool read_numbers(jerry_value_t array,float *out,size_t count) {
    for(size_t i=0;i<count;i++) {
        jerry_value_t value=jerry_object_get_index(array,(uint32_t)i);
        bool valid=jerry_value_is_number(value);
        double number=valid?jerry_value_as_number(value):NAN;
        jerry_value_free(value);
        if(!valid || !isfinite(number) || number < -512 || number > 512) return false;
        out[i]=(float)number;
    }
    return true;
}
static jerry_value_t draw(const jerry_call_info_t *info,const jerry_value_t args[],const jerry_length_t argc) {
    (void)info;
    if(argc!=4 || !jerry_value_is_array(args[0]) || !jerry_value_is_string(args[1]) ||
       !jerry_value_is_array(args[2]) || !jerry_value_is_number(args[3]))
        return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid native Canvas draw arguments");
    size_t count=jerry_array_length(args[0]);
    if(count>MAX_COMMANDS || jerry_array_length(args[2])!=4)
        return jerry_throw_sz(JERRY_ERROR_RANGE,"Canvas draw exceeds native limits");
    if(count==0) return jerry_undefined();
    float ops[MAX_COMMANDS],rgba[4];
    float width=(float)jerry_value_as_number(args[3]);
    if(!read_numbers(args[0],ops,count) || !read_numbers(args[2],rgba,4) ||
       !isfinite(width) || width<=0 || width>640)
        return jerry_throw_sz(JERRY_ERROR_RANGE,"Canvas coordinates/lineWidth exceed prototype limits");
    for(int i=0;i<4;i++) if(rgba[i]<0 || rgba[i]>1)
        return jerry_throw_sz(JERRY_ERROR_RANGE,"Invalid Canvas RGBA");
    for(size_t i=0;i<count;) {
        float op=ops[i++];size_t n=(op==1 || op==2)?2:op==3?0:op==4?4:MAX_COMMANDS+1;
        if(n>count-i) return jerry_throw_sz(JERRY_ERROR_TYPE,"Malformed Canvas path");
        i+=n;
    }
    char mode[7]={0};jerry_size_t len=jerry_string_size(args[1],JERRY_ENCODING_UTF8);
    if(len>6) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid Canvas draw mode");
    jerry_string_to_buffer(args[1],JERRY_ENCODING_UTF8,(jerry_char_t*)mode,len);
    bool stroke=strcmp(mode,"stroke")==0;
    if(!stroke && strcmp(mode,"fill")!=0) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid Canvas draw mode");
    const mcujs_dvi_state_t *state=mcujs_dvi_get_state();
    if(state->initialized && (state->width!=WIDTH || state->height!=HEIGHT))
        return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas display already configured incompatibly");
    if(!mcujs_dvi_is_running()) {
        last_presented=NULL;
        if(!mcujs_dvi_init(WIDTH,HEIGHT) || !mcujs_dvi_start())
            return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas display unavailable");
    }
    uint16_t *pixels=mcujs_dvi_get_draw_buffer();
    if(!pixels) return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas draw surface unavailable");
    /* DVI is double buffered; retain earlier Canvas drawing without a third allocation. */
    if(last_presented && pixels!=last_presented) memcpy(pixels,last_presented,WIDTH*HEIGHT*sizeof(uint16_t));
    if(!canvas_render(pixels,WIDTH,HEIGHT,ops,count,stroke,rgba,width))
        return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas renderer rejected draw");
    if(!mcujs_dvi_swap_and_show()) return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas presentation failed");
    last_presented=pixels;
    return jerry_undefined();
}
static void put(jerry_value_t object,const char *name,jerry_value_t value) {
    jerry_value_t key=jerry_string_sz(name);
    jerry_value_t result=jerry_object_set(object,key,value);
    jerry_value_free(result);jerry_value_free(key);jerry_value_free(value);
}
jerry_value_t js_create_canvas_native_module(void) {
    jerry_value_t module=jerry_object();
    put(module,"width",jerry_number(WIDTH));put(module,"height",jerry_number(HEIGHT));
    put(module,"draw",jerry_function_external(draw));
    return module;
}
