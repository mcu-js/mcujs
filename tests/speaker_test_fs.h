#ifndef SPEAKER_TEST_FS_H
#define SPEAKER_TEST_FS_H
#include "speaker_wav.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char bytes[20000];
static size_t length,position,max_read;static int handles;static fs_result_t access=FS_OK;
fs_result_t fs_open(fs_file_t *f,const char *p,fs_mode_t m){(void)p;assert(m==FS_MODE_READ);if(access)return access;assert(!handles);handles++;f->is_open=true;position=0;return FS_OK;}
fs_result_t fs_close(fs_file_t *f){assert(f->is_open);f->is_open=false;handles--;return FS_OK;}
fs_result_t fs_size(fs_file_t *f,size_t *n){assert(f->is_open);*n=length;return access;}
fs_result_t fs_seek(fs_file_t *f,uint32_t p){assert(f->is_open);position=p;return access;}
fs_result_t fs_read(fs_file_t *f,void *b,size_t n,size_t *r){assert(f->is_open);if(n>max_read)max_read=n;if(access)return access;*r=position<length?(n<length-position?n:length-position):0;if(*r)memcpy(b,bytes+position,*r);position+=*r;return FS_OK;}
static void u16(unsigned p,unsigned n){bytes[p]=n;bytes[p+1]=n>>8;}
static void u32(unsigned p,unsigned n){u16(p,n);u16(p+2,n>>16);}
static void wav_fixture(void){memset(bytes,0,sizeof bytes);memcpy(bytes,"RIFF",4);memcpy(bytes+8,"WAVEJUNK",8);u32(16,1);bytes[20]=42;memcpy(bytes+22,"fmt ",4);u32(26,16);u16(30,1);u16(32,1);u32(34,16000);u32(38,32000);u16(42,2);u16(44,16);memcpy(bytes+46,"data",4);u32(50,8192);length=54+8192;u32(4,length-8);u16(54,32767);u16(56,32768);}
#endif
