/* Private native Canvas instances. No screen-controller logic in this binding. */
#include "jerryscript.h"
#include "canvas_renderer.h"
#include "canvas_display.h"
#include "validation.h"
#include <math.h>
#include <stddef.h>
#if defined(MCUJS_PLATFORM_RP2) && defined(MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69)
#include "touch_169.h"
#define CANVAS_TOUCH_169
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef MCUJS_PLATFORM_RP2
#include <malloc.h>
#endif
#define MAX_COMMANDS 128
static canvas_display_t *displays;
static unsigned presentations, presentation_failures;
static bool default_unavailable, resetting;
typedef struct {
    canvas_display_t display; /* first: backend boundary remains engine-neutral */
    jerry_value_t lifecycle;
    bool touch_capable, touch_started, horizontal;
} native_display_t;
static void stop_touch(canvas_display_t *d) {
    native_display_t *n=(native_display_t *)d;
#ifdef CANVAS_TOUCH_169
    if (n->touch_started) mcujs_touch_169_close();
#endif
    n->touch_started=false;
}

static jerry_value_t display_error(mcujs_operational_error_t code, const char *message) {
    const mcujs_error_details_t details = {.resource="display"};
    return mcujs_throw_operational_error(code, message, &details);
}

static void close_display(canvas_display_t *d) {
    if (!d || d->closed) return;
    canvas_display_t **p=&displays;
    while (*p && *p!=d) p=&(*p)->next;
    if (*p) *p=d->next;
    d->closed=true; d->pending=false;
    if (d->is_default) default_unavailable=d->failed;
    stop_touch(d);
    if (d->release) d->release(d);
}
static void free_display(void *ptr, jerry_object_native_info_t *info) {
    close_display(ptr); jerry_native_ptr_free(ptr,info); free(ptr);
}
static const jerry_object_native_info_t display_type={
    .free_cb=free_display, .number_of_references=1,
    .offset_of_references=offsetof(native_display_t,lifecycle)
};
/* Only explicit VM-valid paths notify; GC releases native resources silently.
 * Copy/clear before calling JS: callbacks may close, allocate, or trigger GC. */
static void close_and_notify(canvas_display_t *d) {
    if (!d || d->closed) return;
    native_display_t *n=(native_display_t *)d;
    jerry_value_t callback=jerry_value_copy(n->lifecycle);
    jerry_native_ptr_set(&n->lifecycle,jerry_undefined());
    close_display(d);
    if (jerry_value_is_function(callback)) {
        jerry_value_t result=jerry_call(callback,jerry_undefined(),NULL,0);
        jerry_value_free(result);
    }
    jerry_value_free(callback);
}
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
    if(!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    if(d->closed) return display_error(MCUJS_ERROR_NO_DEVICE,"Display is closed or unavailable");
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
    close_and_notify(d);return jerry_undefined();
}
static bool present_display(canvas_display_t *d) {
    if (!d->pending) return true;
    if (d->present(d)) { d->pending=false; presentations++; return true; }
    presentation_failures++;
    d->failed=true;
    close_and_notify(d);
    return false;
}
static jerry_value_t present_method(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if (!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    if (d->closed) return display_error(MCUJS_ERROR_NO_DEVICE,"Display is closed or unavailable");
    if (!present_display(d)) return display_error(MCUJS_ERROR_IO,"Display presentation failed; handle released");
    return jerry_undefined();
}
static jerry_value_t get_state(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if (!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    return jerry_string_sz(d->failed ? "error" : d->closed ? "closed" : "open");
}
static jerry_value_t default_state(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_string_sz(displays ? "busy" : default_unavailable ? "unavailable" : "idle");
}
/* Engine teardown, before Jerry finalizers: invalidate handles, release leases. */
void js_canvas_reset(void) {
    if (resetting) return;
    resetting=true;
    while (displays) close_and_notify(displays);
    resetting=false;
    default_unavailable=false;
    presentations=0; presentation_failures=0;
}
static jerry_value_t set_lifecycle(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    canvas_display_t *d=receiver(info->this_value);
    if (!d || argc!=1 || !jerry_value_is_function(args[0]))
        return jerry_throw_sz(JERRY_ERROR_TYPE,"Expected display and lifecycle callback");
    if (d->closed) return display_error(MCUJS_ERROR_NO_DEVICE,"Display is closed");
    jerry_native_ptr_set(&((native_display_t *)d)->lifecycle,args[0]);
    return jerry_undefined();
}
static jerry_value_t stop_pointer(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if (!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    stop_touch(d); return jerry_undefined();
}
static jerry_value_t start_pointer(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if (!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    native_display_t *n=(native_display_t *)d;
    if (d->closed || !n->touch_capable) return display_error(MCUJS_ERROR_NO_DEVICE,"Touch unavailable");
    if (n->touch_started) return jerry_undefined();
#ifdef CANVAS_TOUCH_169
    int status=mcujs_touch_169_open();
    if (status) return display_error(status==1?MCUJS_ERROR_BUSY:MCUJS_ERROR_IO,"Touch initialization failed");
    n->touch_started=true;
#endif
    return jerry_undefined();
}
static jerry_value_t sample_pointer(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)args; (void)argc;
    canvas_display_t *d=receiver(info->this_value);
    if (!d) return jerry_throw_sz(JERRY_ERROR_TYPE,"Invalid display receiver");
    native_display_t *n=(native_display_t *)d;
    if (d->closed || !n->touch_started) return display_error(MCUJS_ERROR_NO_DEVICE,"Touch is stopped");
    bool pressed=false; int x=0,y=0;
#ifdef CANVAS_TOUCH_169
    if (!mcujs_touch_169_sample(n->horizontal,&pressed,&x,&y)) {
        stop_touch(d);
        return display_error(MCUJS_ERROR_IO,mcujs_touch_169_error());
    }
#endif
    jerry_value_t result=jerry_array(3);
    jerry_value_t values[]={jerry_boolean(pressed),jerry_number(x),jerry_number(y)};
    for (unsigned i=0;i<3;i++) {
        jerry_value_t r=jerry_object_set_index(result,i,values[i]);
        jerry_value_free(r); jerry_value_free(values[i]);
    }
    return result;
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
    if (resetting) return display_error(MCUJS_ERROR_BUSY,"Canvas reset in progress");
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
    bool is_default=strcmp(kind,"default")==0;
    /* A configured default lease is exclusive across Canvas connections.
     * External-only connections retain their existing driver-level arbitration. */
    for (canvas_display_t *it=displays; it; it=it->next) {
        if (is_default || it->is_default)
            return display_error(MCUJS_ERROR_BUSY,"Configured display requires exclusive Canvas ownership");
    }
    canvas_display_t *d=calloc(1,sizeof(native_display_t));
    if(!d) {
        if (is_default) default_unavailable=true;
        return display_error(MCUJS_ERROR_RESOURCE_EXHAUSTED,"Out of memory opening display");
    }
    jerry_native_ptr_init(d,&display_type);
    d->is_default=is_default;
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
    if(!ok) {
        if (is_default) default_unavailable=true;
        free(d);
        return display_error(MCUJS_ERROR_IO,"Display initialization failed: unavailable resource, bus conflict, or allocation failure");
    }
    if (is_default) default_unavailable=false;
    jerry_value_t result=jerry_object();jerry_object_set_native_ptr(result,&display_type,d);
    native_display_t *instance=(native_display_t *)d;
#ifdef CANVAS_TOUCH_169
    instance->touch_capable=is_default && lcd && cfg.profile==CANVAS_PANEL_WAVESHARE_1_69;
    instance->horizontal=lcd && cfg.horizontal;
#endif
    put(result,"maxTouchPoints",jerry_number(instance->touch_capable?1:0));
    put(result,"setLifecycle",jerry_function_external(set_lifecycle));
    put(result,"startPointer",jerry_function_external(start_pointer));
    put(result,"stopPointer",jerry_function_external(stop_pointer));
    put(result,"samplePointer",jerry_function_external(sample_pointer));
    d->next=displays;displays=d;
    put(result,"width",jerry_number(d->width));put(result,"height",jerry_number(d->height));
    put(result,"draw",jerry_function_external(draw));put(result,"close",jerry_function_external(close_method));
    put(result,"present",jerry_function_external(present_method));put(result,"getState",jerry_function_external(get_state));
    return result;
}
void js_canvas_present(void) {
    /* Rescan after callbacks: a listener can close/collect another display.
     * Bound work to the entry population: failure callbacks may open and dirty
     * replacements indefinitely. New work can wait for the next engine turn. */
    size_t budget=0;
    for (canvas_display_t *d=displays; d; d=d->next) budget++;
    while (budget--) {
        canvas_display_t *d=displays;
        while (d && !d->pending) d=d->next;
        if (!d) break;
        if(!present_display(d)) printf("Canvas presentation failed; display closed\r\n");
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
    put(module,"stats",jerry_function_external(stats));
    put(module,"defaultState",jerry_function_external(default_state));return module;
}
