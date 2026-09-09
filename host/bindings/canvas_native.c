/* Private native Canvas instances. No screen-controller logic in this binding. */
#include "jerryscript.h"
#include "canvas_renderer.h"
#include "canvas_display.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef MCUJS_PLATFORM_RP2
#include <malloc.h>
#endif
#define MAX_COMMANDS 128
static canvas_display_t *displays;
static unsigned presentations, presentation_failures;

static void close_display(canvas_display_t *d) {
    if (!d || d->closed) return;
    canvas_display_t **p=&displays;
    while (*p && *p!=d) p=&(*p)->next;
    if (*p) *p=d->next;
    d->closed=true; d->pending=false;
    if (d->release) d->release(d);
}
static void free_display(void *ptr, jerry_object_native_info_t *info) {
    (void)info; close_display(ptr); free(ptr);
}
static const jerry_object_native_info_t display_type={ .free_cb=free_display };
static canvas_display_t *receiver(jerry_value_t value) {
    return jerry_value_is_object(value)?jerry_object_get_native_ptr(value,&display_type):NULL;
}
static void put(jerry_value_t object,const char *name,jerry_value_t value) {
    jerry_value_t key=jerry_string_sz(name);
    jerry_value_t result=jerry_object_set(object,key,value);
    jerry_value_free(result);jerry_value_free(key);jerry_value_free(value);
}
static bool read_numbers(jerry_value_t array,float *out,size_t count) {
    for(size_t i=0;i<count;i++) {
        jerry_value_t value=jerry_object_get_index(array,(uint32_t)i);
        bool valid=jerry_value_is_number(value);
        double number=valid?jerry_value_as_number(value):NAN;
        jerry_value_free(value);
        if(!valid || !isfinite(number) || number < -MCUJS_CANVAS_COORD_LIMIT || number > MCUJS_CANVAS_COORD_LIMIT) return false;
        out[i]=(float)number;
    }
    return true;
}
static jerry_value_t draw(const jerry_call_info_t *info,const jerry_value_t args[],const jerry_length_t argc) {
    canvas_display_t *d=receiver(info->this_value);
    if(!d || d->closed) return jerry_throw_sz(JERRY_ERROR_TYPE,"Display is closed or invalid");
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
    uint16_t *pixels=d->acquire(d);
    if(!pixels) return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas surface unavailable");
    if(!canvas_render(pixels,d->width,d->height,ops,count,stroke,rgba,width))
        return jerry_throw_sz(JERRY_ERROR_COMMON,"Canvas renderer rejected draw");
    d->pending=true;
    return jerry_undefined();
}
static jerry_value_t close_method(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if(!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    close_display(d);return jerry_undefined();
}
static bool integer_option(jerry_value_t opts,const char *name,int *out,int lo,int hi) {
    jerry_value_t key=jerry_string_sz(name),v=jerry_object_get(opts,key);jerry_value_free(key);
    if(jerry_value_is_undefined(v)){jerry_value_free(v);return true;}
    double n=jerry_value_is_number(v)?jerry_value_as_number(v):NAN;
    jerry_value_free(v);
    if(!isfinite(n)||n!=floor(n)||n<lo||n>hi)return false;
    *out=(int)n;return true;
}
static bool known_options(jerry_value_t opts) {
    jerry_value_t keys=jerry_object_keys(opts);
    if(jerry_value_is_exception(keys)){jerry_value_free(keys);return false;}
    bool valid=true;
    const char *allowed[]={"profile","spi","sck","mosi","cs","dc","reset","backlight","width","height","baudrate","horizontal"};
    for(uint32_t i=0;i<jerry_array_length(keys);i++) {
        jerry_value_t key=jerry_object_get_index(keys,i);char text[32]={0};
        jerry_size_t n=jerry_string_size(key,JERRY_ENCODING_UTF8);bool found=false;
        if(n<sizeof(text)) {
            jerry_string_to_buffer(key,JERRY_ENCODING_UTF8,(jerry_char_t*)text,n);
            for(size_t j=0;j<sizeof(allowed)/sizeof(*allowed);j++) if(strlen(text)==n && strcmp(text,allowed[j])==0)found=true;
        }
        jerry_value_free(key);if(!found){valid=false;break;}
    }
    jerry_value_free(keys);return valid;
}
static bool lcd_options(jerry_value_t opts,canvas_lcd_config_t *c) {
    *c=(canvas_lcd_config_t){.spi=-1,.sck=-1,.mosi=-1,.cs=-1,.dc=-1,.reset=-1,.backlight=-1,
        .width=320,.height=172,.x_offset=0,.y_offset=34,.baudrate=37500000,.horizontal=true};
#ifdef MCUJS_CANVAS_DEFAULT_ST7789_1_69
    c->spi=1;c->sck=10;c->mosi=11;c->cs=9;c->dc=8;c->reset=13;c->backlight=25;
    c->profile=CANVAS_PANEL_WAVESHARE_1_69;
#elif defined(MCUJS_CANVAS_DEFAULT_ST7789_2_8)
    c->spi=1;c->sck=10;c->mosi=11;c->cs=13;c->dc=14;c->reset=15;c->backlight=16;
    c->profile=CANVAS_PANEL_WAVESHARE_2_8;
#elif defined(MCUJS_CANVAS_DEFAULT_ST7789)
    c->spi=0;c->sck=18;c->mosi=19;c->cs=17;c->dc=16;c->reset=20;c->backlight=21;
#endif
    if(!known_options(opts))return false;
    jerry_value_t key=jerry_string_sz("profile"),v=jerry_object_get(opts,key);jerry_value_free(key);
    if(!jerry_value_is_undefined(v)) {
        if(!jerry_value_is_string(v)){jerry_value_free(v);return false;}
        char profile[32]={0};jerry_size_t n=jerry_string_size(v,JERRY_ENCODING_UTF8);
        if(n>=sizeof(profile)){jerry_value_free(v);return false;}
        jerry_string_to_buffer(v,JERRY_ENCODING_UTF8,(jerry_char_t*)profile,n);
        if(strlen(profile)!=n){jerry_value_free(v);return false;}
        if(strcmp(profile,"waveshare-2.8")==0)c->profile=CANVAS_PANEL_WAVESHARE_2_8;
        else if(strcmp(profile,"waveshare-1.69")==0)c->profile=CANVAS_PANEL_WAVESHARE_1_69;
        else if(strcmp(profile,"waveshare-1.47")==0)c->profile=CANVAS_PANEL_WAVESHARE_1_47;
        else {jerry_value_free(v);return false;}
    }
    jerry_value_free(v);
    c->horizontal=c->profile!=CANVAS_PANEL_WAVESHARE_1_69;
    key=jerry_string_sz("horizontal");v=jerry_object_get(opts,key);jerry_value_free(key);
    if(!jerry_value_is_undefined(v)) {
        if(!jerry_value_is_boolean(v)){jerry_value_free(v);return false;}
        c->horizontal=jerry_value_is_true(v);
    }
    jerry_value_free(v);
    if(c->profile==CANVAS_PANEL_WAVESHARE_1_69) {
        c->width=c->horizontal?280:240;c->height=c->horizontal?240:280;
        c->x_offset=c->horizontal?20:0;c->y_offset=c->horizontal?0:20;
    } else if(c->profile==CANVAS_PANEL_WAVESHARE_2_8) {
        c->width=c->horizontal?320:240;c->height=c->horizontal?240:320;
        c->x_offset=c->y_offset=0;
    } else if(!c->horizontal){c->width=172;c->height=320;c->x_offset=34;c->y_offset=0;}
    int width=c->width,height=c->height;
    if(!integer_option(opts,"spi",&c->spi,0,1) ||
       !integer_option(opts,"sck",&c->sck,0,29) || !integer_option(opts,"mosi",&c->mosi,0,29) ||
       !integer_option(opts,"cs",&c->cs,0,29) || !integer_option(opts,"dc",&c->dc,0,29) ||
       !integer_option(opts,"reset",&c->reset,0,29) || !integer_option(opts,"backlight",&c->backlight,0,29) ||
       !integer_option(opts,"width",&width,1,320) || !integer_option(opts,"height",&height,1,320) ||
       !integer_option(opts,"baudrate",&c->baudrate,100000,37500000))return false;
    if(width!=c->width || height!=c->height || c->spi<0 || c->sck<0 || c->mosi<0 ||
       c->cs<0 || c->dc<0 || c->reset<0 || c->backlight<0)return false;
    if(c->sck%4!=2 || c->mosi%4!=3 || (c->sck/8)%2!=c->spi || (c->mosi/8)%2!=c->spi)return false;
    int pins[]={c->sck,c->mosi,c->cs,c->dc,c->reset,c->backlight};
    for(int i=0;i<6;i++)for(int j=i+1;j<6;j++)if(pins[i]==pins[j])return false;
    return true;
}
static jerry_value_t open_method(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;
    if(argc!=2 || !jerry_value_is_string(args[0]) || !jerry_value_is_object(args[1]) || jerry_value_is_array(args[1]))
        return jerry_throw_sz(JERRY_ERROR_TYPE,"Expected display kind and options object");
    char kind[16]={0};jerry_size_t n=jerry_string_size(args[0],JERRY_ENCODING_UTF8);
    if(n>=sizeof(kind))return jerry_throw_sz(JERRY_ERROR_TYPE,"Unsupported display driver");
    jerry_string_to_buffer(args[0],JERRY_ENCODING_UTF8,(jerry_char_t*)kind,n);
    if(strlen(kind)!=n)return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display driver name");
    bool epaper=false;
#if defined(MCUJS_CANVAS_EPAPER154) || defined(MCUJS_CANVAS_STICKY)
    epaper=strcmp(kind,"default")==0;
#endif
    bool dvi=false,lcd=strcmp(kind,"st7789")==0;
    if(strcmp(kind,"default")==0) {
#ifdef MCUJS_CANVAS_DVI
        dvi=true;
#elif defined(MCUJS_CANVAS_DEFAULT_ST7789)
        lcd=true;
#endif
    }
    if(!dvi && !lcd && !epaper)return jerry_throw_sz(JERRY_ERROR_TYPE,"Unsupported display driver");
    canvas_lcd_config_t cfg;
    if(lcd && !lcd_options(args[1],&cfg))return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid ST7789 panel profile or SPI connection options");
    if(dvi || epaper) {
        jerry_value_t keys=jerry_object_keys(args[1]);
        bool valid=!jerry_value_is_exception(keys) && jerry_array_length(keys)==0;jerry_value_free(keys);
        if(!valid)return jerry_throw_sz(JERRY_ERROR_TYPE,"Default display takes no options");
    }
    canvas_display_t *d=calloc(1,sizeof(*d));
    if(!d)return jerry_throw_sz(JERRY_ERROR_COMMON,"Out of memory opening display");
    bool ok=false;
#ifdef MCUJS_CANVAS_DVI
    if(dvi)ok=canvas_display_dvi_init(d);
#endif
#ifdef MCUJS_CANVAS_EPAPER154
    if(epaper)ok=canvas_display_epaper154_init(d);
#elif defined(MCUJS_CANVAS_STICKY)
    if(epaper)ok=canvas_display_sticky_init(d);
#else
    if(lcd)ok=canvas_display_st7789_init(d,&cfg);
#endif
    if(!ok){free(d);return jerry_throw_sz(JERRY_ERROR_COMMON,"Display unavailable, conflicting connection, or insufficient memory");}
    jerry_value_t result=jerry_object();jerry_object_set_native_ptr(result,&display_type,d);
    d->next=displays;displays=d;
    put(result,"width",jerry_number(d->width));put(result,"height",jerry_number(d->height));
    put(result,"draw",jerry_function_external(draw));put(result,"close",jerry_function_external(close_method));
    return result;
}
void js_canvas_present(void) {
    for(canvas_display_t *d=displays,*next;d;d=next) {
        next=d->next;
        if(!d->pending)continue;
        if(d->present(d)){d->pending=false;presentations++;}
        else {presentation_failures++;close_display(d);printf("Canvas presentation failed; display closed\r\n");}
    }
}
static jerry_value_t stats(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;(void)args;(void)argc;jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
    jerry_heap_stats_t memory;
    jerry_value_t result=jerry_object();
    /* Presentation counters do not depend on optional allocator telemetry. */
    if(jerry_heap_stats(&memory)) {
        put(result,"jsUsed",jerry_number(memory.allocated_bytes));put(result,"jsPeak",jerry_number(memory.peak_allocated_bytes));
        put(result,"jsTotal",jerry_number(memory.size));
    }
#ifdef MCUJS_PLATFORM_RP2
    put(result,"nativeAllocated",jerry_number(mallinfo().uordblks));
#endif
    put(result,"presentations",jerry_number(presentations));put(result,"presentationFailures",jerry_number(presentation_failures));
    return result;
}
jerry_value_t js_create_canvas_native_module(void) {
    jerry_value_t module=jerry_object();put(module,"open",jerry_function_external(open_method));
    put(module,"stats",jerry_function_external(stats));return module;
}
