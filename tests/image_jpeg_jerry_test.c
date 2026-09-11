/* Real picojpeg + public Jerry factories, 64 KiB JS heap. No hardware access. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jerryscript.h"
#include "bindings.h"
#include "image.h"
#include "jpeg.h"
#include "fs.h"
static const char *phase;
static unsigned live, alloc_calls, graphics_calls;
static int fail_at = -1;
void *jpeg_test_malloc(size_t n) {
    if ((int)alloc_calls++ == fail_at) return NULL;
    void *p = malloc(n); if (p) live++; return p;
}
void *jpeg_test_calloc(size_t n, size_t z) {
    if ((int)alloc_calls++ == fail_at) return NULL;
    void *p = calloc(n,z); if (p) live++; return p;
}
void jpeg_test_free(void *p) { if(p) {assert(live);live--;free(p);} }
/* Legacy boundaries: any unexpected attempt is visible and cannot access IO. */
bool graphics_buffer_valid(graphics_buffer_handle_t h) { (void)h; graphics_calls++; return false; }
bool graphics_get_buffer_info(graphics_buffer_handle_t h, graphics_buffer_info_t *i) { (void)h;(void)i;graphics_calls++;return false; }
uint16_t *graphics_get_buffer_data(graphics_buffer_handle_t h) { (void)h;graphics_calls++;return NULL; }
fs_result_t fs_open(fs_file_t *f,const char *p,fs_mode_t m){(void)f;(void)p;(void)m;return FS_ERROR;}
fs_result_t fs_close(fs_file_t *f){(void)f;return FS_ERROR;}
fs_result_t fs_size(fs_file_t *f,size_t *s){(void)f;(void)s;return FS_ERROR;}
fs_result_t fs_read(fs_file_t *f,void *b,size_t n,size_t *r){(void)f;(void)b;(void)n;(void)r;return FS_ERROR;}

static void check(jerry_value_t v) {
    if (!jerry_value_is_exception(v)) return;
    jerry_value_t e=jerry_exception_value(v,false), s=jerry_value_to_string(e);
    char text[512]={0};jerry_string_to_buffer(s,JERRY_ENCODING_UTF8,(jerry_char_t*)text,sizeof(text)-1);
    fprintf(stderr,"FAIL %s: %s\n",phase,text);exit(1);
}
static void run(const char *s) {jerry_value_t v=jerry_eval((const jerry_char_t*)s,strlen(s),0);check(v);jerry_value_free(v);}
static void publish(const char *name,jerry_value_t v) {
    jerry_value_t g=jerry_current_realm(),r=jerry_object_set_sz(g,name,v);check(r);
    jerry_value_free(r);jerry_value_free(g);jerry_value_free(v);
}
static void collect(void) {jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);assert(!jpeg_decoder_busy());assert(live==0);}
static unsigned number(jerry_value_t o,const char *key) {
    jerry_value_t v=jerry_object_get_sz(o,key);assert(jerry_value_is_number(v));
    double n=jerry_value_as_number(v);assert(n>=0&&n<=320);jerry_value_free(v);return (unsigned)n;
}
static unsigned char *file(const char *root,const char *name,size_t *size) {
    char path[1024];assert(snprintf(path,sizeof(path),"%s/%s",root,name)<(int)sizeof(path));
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&n<200000);
    rewind(f);unsigned char *b=malloc((size_t)n);assert(b);assert(fread(b,1,(size_t)n,f)==(size_t)n);assert(!fclose(f));*size=(size_t)n;return b;
}
static jerry_value_t bytes(const unsigned char *p,size_t n) {
    jerry_value_t v=jerry_typedarray(JERRY_TYPEDARRAY_UINT8,(jerry_length_t)n);check(v);
    jerry_value_t b=jerry_typedarray_buffer(v,NULL,NULL);if(n)memcpy(jerry_arraybuffer_data(b),p,n);
    jerry_value_free(b);return v;
}
static jerry_value_t get_global(const char *name) {jerry_value_t g=jerry_current_realm(),v=jerry_object_get_sz(g,name);jerry_value_free(g);return v;}
static jerry_value_t open_bytes(const unsigned char *p,size_t n) {
    jerry_value_t m=get_global("jpeg"),f=jerry_object_get_sz(m,"open"),a=bytes(p,n),r=jerry_call(f,m,&a,1);
    jerry_value_free(a);jerry_value_free(f);jerry_value_free(m);return r;
}
static unsigned char *decode(const unsigned char *p,size_t n,unsigned w,unsigned h,int mutate) {
    publish("bytes",bytes(p,n));
    run("var reader=jpeg.open(bytes), target=new Uint8Array(new ArrayBuffer(800),16,768);");
    if(mutate)run("bytes.fill(0); bytes=null;");
    jerry_value_t r=get_global("reader"),t=get_global("target"),b=jerry_typedarray_buffer(t,NULL,NULL),f=jerry_object_get_sz(r,"read");
    assert(number(r,"width")==w&&number(r,"height")==h);
    unsigned char *raw=jerry_arraybuffer_data(b),*out=calloc(w*h,3),*covered=calloc(w*h,1);assert(out&&covered);
    unsigned blocks=0;
    for(;;){
        memset(raw,0xa5,800);jerry_value_t block=jerry_call(f,r,&t,1);check(block);
        if(jerry_value_is_null(block)){jerry_value_free(block);break;}
        unsigned x=number(block,"x"),y=number(block,"y"),bw=number(block,"width"),bh=number(block,"height");
        assert(bw&&bh&&bw<=16&&bh<=16&&x+bw<=w&&y+bh<=h&&++blocks<=w*h);
        for(unsigned k=0;k<16;k++)assert(raw[k]==0xa5);
        for(unsigned k=16+bw*bh*3;k<800;k++)assert(raw[k]==0xa5);
        for(unsigned yy=0;yy<bh;yy++)for(unsigned xx=0;xx<bw;xx++){
            unsigned dst=(y+yy)*w+x+xx;assert(!covered[dst]);covered[dst]=1;
            memcpy(out+dst*3,raw+16+(yy*bw+xx)*3,3);
        }
        jerry_value_free(block);
    }
    for(unsigned i=0;i<w*h;i++) assert(covered[i]);
    free(covered);
    assert(!jpeg_decoder_busy());
    run("if(reader.read(target)!==null)throw Error('EOF not stable'); reader.close();reader.close();"
        "expectError(function(){reader.read(target)},'ENXIO'); reader=null;target=null;bytes=null;");
    jerry_value_free(f);jerry_value_free(b);jerry_value_free(t);jerry_value_free(r);collect();return out;
}
static void malformed(const unsigned char *p,size_t n) {
    jerry_value_t r=open_bytes(p,n);
    if(!jerry_value_is_exception(r)){
        jerry_value_t t=jerry_typedarray(JERRY_TYPEDARRAY_UINT8,768),f=jerry_object_get_sz(r,"read");
        bool failed=false;
        for(unsigned i=0;i<1601;i++){
            jerry_value_t block=jerry_call(f,r,&t,1);
            if(jerry_value_is_exception(block)){failed=true;jerry_value_free(block);break;}
            bool end=jerry_value_is_null(block);jerry_value_free(block);if(end)break;
        }
        jerry_value_free(t);jerry_value_free(f);
        if(!failed){fprintf(stderr,"FAIL %s: truncated input accepted (%zu bytes)\n",phase,n);exit(1);}
    }
    jerry_value_free(r);collect();
}
static void bad_segment(const unsigned char *p,size_t n,unsigned marker,const unsigned char *payload,size_t size) {
    unsigned char *q=malloc(n+size+4);assert(q);
    memcpy(q,p,2);q[2]=0xff;q[3]=marker;q[4]=(size+2)>>8;q[5]=(size+2)&255;
    if(size)memcpy(q+6,payload,size);
    memcpy(q+6+size,p+2,n-2);
    malformed(q,n+size+4);free(q);
}
static void bad_terminal(const unsigned char *p,size_t n,const unsigned char *extra,size_t size) {
    unsigned char *q=malloc(n+size);assert(q);memcpy(q,p,n-2);memcpy(q+n-2,extra,size);
    memcpy(q+n-2+size,p+n-2,2);malformed(q,n+size);free(q);
}
int main(int argc,char **argv) {
    assert(argc==2);const char *root=argv[1];size_t n;unsigned char *p;
    for(unsigned cycle=0;cycle<2;cycle++){
        phase="factory";jerry_init(JERRY_INIT_EMPTY);publish("jpeg",js_create_jpeg_module());
        run("if(typeof jpeg.open!=='function')throw Error('public jpeg.open missing');"
            "function expectError(fn,code){var e;try{fn()}catch(x){e=x}if(!e||e.code!==code)throw Error('Expected '+code+', got '+e)}");
        const char *names[]={"rgb444","rgb422","rgb420","restart","blue444","blue420","gray","one"};
        for(unsigned k=0;k<8;k++){
            phase=names[k];char name[128];snprintf(name,sizeof(name),"tests/jpeg_fixtures/%s.jpg",phase);p=file(root,name,&n);
            unsigned w=k==7?1:17,h=k==7?1:19;unsigned char *decoded=decode(p,n,w,h,1);free(p);
            snprintf(name,sizeof(name),"tests/jpeg_fixtures/%s.rgb",phase);size_t oracle_n;unsigned char *oracle=file(root,name,&oracle_n);assert(oracle_n==w*h*3);
            unsigned max=0;for(size_t i=0;i<oracle_n;i++){unsigned d=(unsigned)abs((int)decoded[i]-oracle[i]);if(d>max)max=d;}
            printf("%s: %ux%u independent RGB max difference %u\n",phase,w,h,max);fflush(stdout);
            assert(max<=(k>=4?2u:5u));free(oracle);free(decoded);
        }
        phase="known 172x320 JPEG";p=file(root,"examples/images/test_172x320.jpg",&n);
        unsigned char *decoded=decode(p,n,172,320,0);
        const char *proof=getenv("JPEG_TEST_RGB_OUT");
        if(proof){FILE *f=fopen(proof,"wb");assert(f);assert(fwrite(decoded,1,172*320*3,f)==172*320*3);assert(!fclose(f));}
        const unsigned coords[][2]={{0,0},{86,0},{86,160},{171,319}};
        const unsigned expected[][3]={{254,0,2},{254,0,2},{120,6,120},{1,0,252}};
        for(unsigned i=0;i<4;i++)for(unsigned c=0;c<3;c++)assert(abs((int)decoded[(coords[i][1]*172+coords[i][0])*3+c]-(int)expected[i][c])<=5);
        free(decoded);free(p);
        phase="malformed marker contents";
        p=file(root,"tests/jpeg_fixtures/one.jpg",&n);
        bad_segment(p,n,0xdd,NULL,0);
        bad_segment(p,n,0xc4,NULL,0);bad_segment(p,n,0xdb,NULL,0);
        unsigned char huff[30]={0};huff[0]=2;huff[1]=1;
        bad_segment(p,n,0xc4,huff,18); /* unsupported table id, otherwise complete */
        huff[0]=0;huff[1]=13;bad_segment(p,n,0xc4,huff,30);
        huff[1]=3;huff[17]=0;huff[18]=1;huff[19]=2;bad_segment(p,n,0xc4,huff,20);
        huff[1]=2;bad_segment(p,n,0xc4,huff,19); /* forbidden all-one code */
        huff[1]=1;huff[17]=12;bad_segment(p,n,0xc4,huff,18);
        huff[0]=0x10;huff[17]=0x10;bad_segment(p,n,0xc4,huff,18);
        huff[17]=0x0b;bad_segment(p,n,0xc4,huff,18);
        unsigned char quant[129];memset(quant,1,sizeof(quant));quant[0]=2;
        bad_segment(p,n,0xdb,quant,65);quant[0]=0;quant[1]=0;bad_segment(p,n,0xdb,quant,65);
        quant[0]=0x10;quant[1]=1;bad_segment(p,n,0xdb,quant,129);
        phase="exact entropy termination";
        const unsigned char extra_zero[]={0},extra_stuffed[]={0xff,0},extra_rst[]={0xff,0xd0};
        bad_terminal(p,n,extra_zero,sizeof(extra_zero));bad_terminal(p,n,extra_stuffed,sizeof(extra_stuffed));
        bad_terminal(p,n,extra_rst,sizeof(extra_rst));
        assert(p[n-3]==0x2b);p[n-3]&=~1u;malformed(p,n);free(p);
        phase="legal terminal marker fill and opaque APP payload";
        p=file(root,"tests/jpeg_fixtures/one.jpg",&n);
        unsigned char *q=malloc(n+10);assert(q);memcpy(q,p,n-2);q[n-2]=0xff;memcpy(q+n-1,p+n-2,2);
        decoded=decode(q,n+1,1,1,1);for(unsigned k=0;k<3;k++)assert(decoded[k]==128);free(decoded);
        const unsigned char app[]={0xff,0xed,0,8,0xff,0xd9,0xff,0xd0,0xff,0};
        memcpy(q,p,2);memcpy(q+2,app,sizeof(app));memcpy(q+2+sizeof(app),p+2,n-2);
        decoded=decode(q,n+sizeof(app),1,1,1);for(unsigned k=0;k<3;k++)assert(decoded[k]==128);free(decoded);free(q);free(p);
        phase="restart padding, exact boundaries and marker fill";
        p=file(root,"tests/jpeg_fixtures/restartgray.jpg",&n);
        const unsigned char suffix[]={0x2b,0xff,0xd0,0x2b,0xff,0xd9};assert(!memcmp(p+n-sizeof(suffix),suffix,sizeof(suffix)));
        decoded=decode(p,n,1,9,1);for(unsigned k=0;k<27;k++)assert(decoded[k]==128);free(decoded);
        q=malloc(n+1);assert(q);memcpy(q,p,n-5);q[n-5]=0;memcpy(q+n-4,p+n-5,5);malformed(q,n+1);
        q[n-5]=0xff;decoded=decode(q,n+1,1,9,1);for(unsigned k=0;k<27;k++)assert(decoded[k]==128);free(decoded);free(q);
        p[n-6]=0x2a;malformed(p,n);p[n-6]=0x2b;p[n-4]=0xd1;malformed(p,n);free(p);
        phase="single scan component coverage and order";
        p=file(root,"tests/jpeg_fixtures/rgb444.jpg",&n);assert(p[609]==0xff&&p[610]==0xda&&p[613]==3);
        q=malloc(n);assert(q);memcpy(q,p,616);q[612]=8;q[613]=1;memcpy(q+616,p+620,n-620);malformed(q,n-4);
        memcpy(q,p,n);q[614]=p[616];q[615]=p[617];q[616]=p[614];q[617]=p[615];malformed(q,n);
        free(q);free(p);
        phase="non-baseline SOS";
        p=file(root,"tests/jpeg_fixtures/one.jpg",&n);assert(n==331&&p[327]==0);
        assert(p[325]==0&&p[326]==63);
        const unsigned bad_sos[][2]={{327,0x10},{325,1},{326,0},{326,64},{327,1},{327,0x11}};
        for(unsigned k=0;k<sizeof(bad_sos)/sizeof(bad_sos[0]);k++){
            unsigned at=bad_sos[k][0];unsigned char saved=p[at];p[at]=bad_sos[k][1];malformed(p,n);p[at]=saved;
        }free(p);
        phase="inherited reader setter retains lease";
        p=file(root,"tests/jpeg_fixtures/one.jpg",&n);publish("bytes",bytes(p,n));free(p);
        run("Object.defineProperty(Object.prototype,'width',{configurable:true,set:function(){throw this;}});"
            "jpeg.open(bytes);delete Object.prototype.width;");collect();
        phase="own JPEG fields bypass all inherited setters";
        run("var setterCalls=0;var fields=['open','width','height','read','close','x','y','code'];"
            "fields.forEach(function(k){Object.defineProperty(Object.prototype,k,{configurable:true,set:function(){setterCalls++;throw this;}})});");
        publish("jpeg",js_create_jpeg_module());
        run("var r=jpeg.open(bytes);var b=r.read(new Uint8Array(768));"
            "if(r.width!==1||r.height!==1||b.x!==0||b.y!==0||b.width!==1||b.height!==1)throw Error('own metadata lost');"
            "r.close();r=null;b=null;expectError(function(){jpeg.open(null)},'ERR_INVALID_ARG_TYPE');"
            "fields.forEach(function(k){delete Object.prototype[k]});if(setterCalls)throw Error('inherited setter called');");collect();
        phase="invalid frames";
        const char *bad[]={"progressive","oversize"};
        for(unsigned k=0;k<2;k++){char name[128];snprintf(name,sizeof(name),"tests/jpeg_fixtures/%s.jpg",bad[k]);p=file(root,name,&n);malformed(p,n);free(p);}
        p=file(root,"tests/jpeg_fixtures/rgb420.jpg",&n);
        phase="all prefixes";for(size_t i=0;i<n;i++)malformed(p,i);
        phase="forged EOI truncation";
        unsigned char *cut=malloc(n);assert(cut);
        for(size_t i=2;i<n-2;i++){memcpy(cut,p,i);cut[i]=0xff;cut[i+1]=0xd9;malformed(cut,i+2);}free(cut);
        publish("bytes",bytes(p,n));
        phase="types and receivers";
        run("[null,undefined,1,'x',{},new ArrayBuffer(8),new Uint16Array(8)].forEach(function(v){expectError(function(){jpeg.open(v)},'ERR_INVALID_ARG_TYPE')});"
            "expectError(function(){jpeg.open(new Uint8Array(0))},'ERR_OUT_OF_RANGE');"
            "expectError(function(){jpeg.open(new Uint8Array(16385))},'ERR_OUT_OF_RANGE');"
            "[null,undefined,1,'x',{}].forEach(function(v){expectError(function(){jpeg.open.call(v,bytes)},'ERR_INVALID_ARG_TYPE')});"
            "var reader=jpeg.open(bytes);[null,undefined,1,'x',{}].forEach(function(v){expectError(function(){reader.read.call(v,new Uint8Array(768))},'ERR_INVALID_ARG_TYPE');expectError(function(){reader.close.call(v)},'ERR_INVALID_ARG_TYPE')});"
            "reader.close();reader=null;");collect();
        phase="reader busy and legacy guard";
        run("var reader=jpeg.open(bytes);expectError(function(){jpeg.open(bytes)},'EBUSY');");
        assert(image_decode_jpeg(1,p,n,0,0)==IMAGE_JPEG_ERR_BUSY);assert(graphics_calls==0);
        run("reader.close();reader=null;");collect();
        phase="bad target retires reader";
        run("[null,undefined,1,'x',new Uint16Array(768)].forEach(function(v){var r=jpeg.open(bytes);expectError(function(){r.read(v)},'ERR_INVALID_ARG_TYPE');expectError(function(){r.read(new Uint8Array(768))},'ENXIO');r.close()});"
            "var reader=jpeg.open(bytes);expectError(function(){reader.read(new Uint8Array(767))},'ERR_OUT_OF_RANGE');reader.close();reader=null;");collect();
        phase="snapshot and detached input";
        run("var reader=jpeg.open(bytes);");jerry_value_t in=get_global("bytes"),ab=jerry_typedarray_buffer(in,NULL,NULL),ret=jerry_arraybuffer_detach(ab);check(ret);jerry_value_free(ret);jerry_value_free(ab);jerry_value_free(in);
        run("reader.read(new Uint8Array(768));reader.close();reader=null;expectError(function(){jpeg.open(bytes)},'ERR_INVALID_ARG_TYPE');");collect();publish("bytes",bytes(p,n));
        phase="native allocation failure";
        for(int i=0;i<2;i++){alloc_calls=0;fail_at=i;run("expectError(function(){jpeg.open(bytes)},'ENOMEM');");fail_at=-1;collect();}
        phase="module GC preserves reader";
        run("var reader=jpeg.open(bytes);jpeg=null;");jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);assert(jpeg_decoder_busy());assert(live==2);
        run("reader.read(new Uint8Array(768));reader.close();reader=null;");collect();publish("jpeg",js_create_jpeg_module());
        phase="retained heap and VM teardown";jerry_heap_stats_t before,after;jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);assert(jerry_heap_stats(&before));
        for(unsigned i=0;i<80;i++){run("var reader=jpeg.open(bytes);reader.read(new Uint8Array(768));reader.close();reader=null;");collect();}
        assert(before.size>=63u*1024u&&before.size<=64u*1024u);
        assert(jerry_heap_stats(&after));assert(after.allocated_bytes<=before.allocated_bytes+128);
        printf("64KiB heap cycle %u: retained %zu -> %zu\n",cycle,before.allocated_bytes,after.allocated_bytes);
        run("var reader=jpeg.open(bytes);");assert(jpeg_decoder_busy());jerry_cleanup();assert(!jpeg_decoder_busy()&&live==0);free(p);
    }
    puts("JPEG native pixels, bounds, errors, ownership, GC and VM reinitialization PASS");return 0;
}
