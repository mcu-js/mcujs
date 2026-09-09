/* HDMI adapter: keep framebuffer ownership and copying out of the rasterizer. */
#include "canvas_display.h"
#include "mcujs_dvi.h"
#include <stdlib.h>
#include <string.h>
typedef struct { const uint16_t *last; bool started, fresh; } dvi_surface_t;
static canvas_display_t *owner;
static uint16_t *acquire(canvas_display_t *d) {
    dvi_surface_t *s=d->state;
    if(!mcujs_dvi_is_running()) {
        if(s->started)return NULL; /* Do not silently restart a stopped video worker. */
        if(!mcujs_dvi_init(d->width,d->height)||!mcujs_dvi_start())return NULL;
    }
    s->started=true;
    uint16_t *pixels=mcujs_dvi_get_draw_buffer();if(!pixels)return NULL;
    if(s->fresh){memset(pixels,0,d->width*d->height*2);s->fresh=false;}
    else if(!d->pending && s->last && s->last!=pixels)memcpy(pixels,s->last,d->width*d->height*2);
    return pixels;
}
static bool present(canvas_display_t *d) {
    dvi_surface_t *s=d->state;uint16_t *pixels=mcujs_dvi_get_draw_buffer();
    if(!pixels||!mcujs_dvi_is_running()||!mcujs_dvi_swap_and_show())return false;
    s->last=pixels;return true;
}
static void release(canvas_display_t *d) {
    /* Release Canvas ownership, not the live video worker. It retains its frame. */
    if(owner==d)owner=NULL;free(d->state);d->state=NULL;
}
bool canvas_display_dvi_init(canvas_display_t *d) {
    if(owner)return false;
    const mcujs_dvi_state_t *state=mcujs_dvi_get_state();
    if(state->initialized && (state->width!=160 || state->height!=120))return false;
    dvi_surface_t *s=calloc(1,sizeof(*s));if(!s)return false;s->fresh=true;
    d->width=160;d->height=120;d->state=s;d->acquire=acquire;d->present=present;d->release=release;owner=d;
    return true;
}
