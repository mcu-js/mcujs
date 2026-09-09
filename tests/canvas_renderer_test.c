#include "canvas_renderer.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
#ifdef MCUJS_CANVAS_STICKY
    static uint16_t full[800*480];
    const float white[4]={1,1,1,1}, black[4]={0,0,0,1};
    const float full_rect[]={4,0,0,800,480};
    assert(canvas_render(full,800,480,full_rect,5,false,white,1));
    for (int i=0;i<800*480;i++) assert(full[i]==0xffff);
    const float edge_rect[]={4,790,470,10,10};
    assert(canvas_render(full,800,480,edge_rect,5,false,black,1));
    assert(full[470*800+790]==0 && full[383999]==0 && full[469*800+799]==0xffff);
    const float triangle[]={1,600,100,2,780,100,2,780,300,3};
    assert(canvas_render(full,800,480,triangle,10,false,black,1));
    assert(full[150*800+750]==0 && full[150*800+601]==0xffff);
    const float stroke[]={1,600,400,2,799,400};
    assert(canvas_render(full,800,480,stroke,6,true,black,2));
    assert(full[400*800+790]==0 && full[403*800+790]==0xffff);
    const float invalid[]={4,1025,0,1,1};
    assert(!canvas_render(full,800,480,invalid,5,false,black,1));
    assert(!canvas_render(full,801,480,full_rect,5,false,white,1));
    puts("PASS Sticky ctx: all 384000 RGB565 pixels, far-edge rectangle, polygon, stroke, out-of-profile rejection");
    return 0;
#else
    uint16_t pixels[16*16];
    for (int i=0;i<256;i++) pixels[i]=0x001f;
    const float red[4]={1,0,0,1};
    const float rectangle[]={4,2,3,6,4};
    assert(canvas_render(pixels,16,16,rectangle,5,false,red,1));
    assert(pixels[4*16+4]==0xf800);
    assert(pixels[0]==0x001f);
    assert(pixels[8*16+8]==0x001f);
    const float white[4]={1,1,1,1};
    const float line[]={1,2,10,2,14,10};
    assert(canvas_render(pixels,16,16,line,6,true,white,2));
    fprintf(stderr,"stroke pixels: %04x %04x %04x %04x %04x\n",pixels[8*16+8],pixels[9*16+8],pixels[10*16+8],pixels[11*16+8],pixels[12*16+8]);
    assert(pixels[10*16+8]==0xffff);
    assert(pixels[14*16+8]==0x001f);
    puts("ctx rasterizer: literal RGB565 rectangle and stroke assertions passed");
#endif
}
