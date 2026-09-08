/* ctx.graphics 0.1.18 supplies rasterization; this is an MCU memory profile. */
#include "canvas_renderer.h"
#include <math.h>
#define CTX_IMPLEMENTATION
#define CTX_RASTERIZER 1
#define CTX_RASTERIZER_AA 3
#define CTX_EVENTS 0
#define CTX_THREADS 0
#define CTX_PARSER 0
#define CTX_FORMATTER 0
#define CTX_VT 0
#define CTX_PTY 0
#define CTX_TERM 0
#define CTX_SDL 0
#define CTX_KMS 0
#define CTX_FB 0
#define CTX_SOCKETS 0
#define CTX_FONTS_FROM_FILE 0
#define CTX_FONT_ENGINE_CTX 1
#define CTX_FONT_ENGINE_STB 0
/* Upstream 0.1.18 references gradient state even when drawing solid colours. */
#define CTX_GRADIENTS 1
#define CTX_MAX_GRADIENT_STOPS 2
#define CTX_GRADIENT_CACHE 0
#define CTX_ENABLE_CLIP 0
#define CTX_ENABLE_SHADOW_BLUR 0
#define CTX_ENABLE_CMYK 0
#define CTX_LIMIT_FORMATS 1
#define CTX_ENABLE_RGB565 1
/* General antialiased RGB565 paths use the RGBA8 compositor internally. */
#define CTX_ENABLE_RGBA8 1
#define CTX_MAX_SCANLINES 320
#define CTX_MAX_SCANLINE_LENGTH 320
#define CTX_MIN_EDGE_LIST_SIZE 128
#define CTX_MAX_EDGE_LIST_SIZE 512
#define CTX_MIN_JOURNAL_SIZE 64
#define CTX_MAX_JOURNAL_SIZE 512
#define CTX_MAX_STATES 2
#define CTX_MAX_EDGES 128
#define CTX_MAX_PENDING 64
#define CTX_MAX_TEXTURES 1
#define CTX_MAX_KEYDB 8
#define CTX_MAX_FONTS 1
#include <ctx.h>

bool canvas_render(uint16_t *pixels, int width, int height, const float *ops,
                   size_t count, bool stroke, const float rgba[4], float line_width) {
    if (!pixels || width<1 || width>320 || height<1 || height>320 ||
        !ops || count>128 || !rgba || !isfinite(line_width) || line_width<=0 || line_width>640)
        return false;
    for (size_t i=0;i<count;i++) if (!isfinite(ops[i]) || ops[i]<-512 || ops[i]>512) return false;
    for (int i=0;i<4;i++) if (!isfinite(rgba[i]) || rgba[i]<0 || rgba[i]>1) return false;
    for (size_t i=0;i<count;) {
        float op=ops[i++];
        size_t n=(op==1 || op==2)?2:op==3?0:op==4?4:513;
        if (n>count-i) return false;
        i+=n;
    }
    Ctx *ctx=ctx_new_for_framebuffer(pixels,width,height,width*2,CTX_FORMAT_RGB565);
    if (!ctx) return false;
    /* A framebuffer context is already initialized. start_frame would reset
     * its clip bounds to zero in ctx 0.1.18, suppressing general paths. */
    ctx_rgba(ctx,rgba[0],rgba[1],rgba[2],rgba[3]);
    ctx_rgba_stroke(ctx,rgba[0],rgba[1],rgba[2],rgba[3]);
    ctx_line_width(ctx,line_width);
    ctx_line_cap(ctx,CTX_CAP_NONE);
    ctx_line_join(ctx,CTX_JOIN_MITER);
    ctx_miter_limit(ctx,10);
    ctx_reset_path(ctx);
    for (size_t i=0;i<count;) {
        int op=(int)ops[i++];
        switch(op) {
          case 1: ctx_move_to(ctx,ops[i],ops[i+1]);i+=2;break;
          case 2: ctx_line_to(ctx,ops[i],ops[i+1]);i+=2;break;
          case 3: ctx_close_path(ctx);break;
          case 4: ctx_rectangle(ctx,ops[i],ops[i+1],ops[i+2],ops[i+3]);i+=4;break;
        }
    }
#ifdef CANVAS_RENDER_TRACE
    CtxRasterizer *r=(CtxRasterizer*)ctx->backend;
    fprintf(stderr,"trace stroke=%d edges=%d width=%f path=%d\n",stroke,r->edge_list.count,ctx_get_line_width(ctx),ctx->current_path.count);
    for(int j=0;j<r->edge_list.count && j<6;j++){CtxSegment *e=&((CtxSegment*)r->edge_list.entries)[j];fprintf(stderr,"edge %d code=%d x0=%d y0=%d x1=%d y1=%d\n",j,e->code,e->x0,e->y0,e->x1,e->y1);}
#endif
    if (stroke) ctx_stroke(ctx); else ctx_fill(ctx);
    ctx_end_frame(ctx);
    ctx_destroy(ctx);
    return true;
}
