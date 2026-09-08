#include "canvas_renderer.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
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
}
