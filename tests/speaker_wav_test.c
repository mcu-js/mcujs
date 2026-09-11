#include "speaker_test_fs.h"
static void rejected(speaker_wav_t *w,speaker_wav_result_t expected){assert(speaker_wav_open(w,"/app/chime.wav")==expected);assert(!handles&&!w->file.is_open);}
int main(void){speaker_wav_t w={0};uint32_t frames[512];size_t count;
 wav_fixture();assert(speaker_wav_open(&w,"/app/chime.wav")==SPEAKER_WAV_OK);assert(handles==1);
 assert(speaker_wav_read(&w,frames,512,0.25,&count)==SPEAKER_WAV_OK);assert(count==512);assert(frames[0]==0x1fff1fff);assert(frames[1]==0xe000e000);
 while(w.remaining)assert(speaker_wav_read(&w,frames,512,1,&count)==SPEAKER_WAV_OK);
 assert(speaker_wav_read(&w,frames,512,1,&count)==SPEAKER_WAV_OK&&count==0);speaker_wav_close(&w);assert(!handles&&max_read<=1024);
 wav_fixture();u16(32,2);rejected(&w,SPEAKER_WAV_UNSUPPORTED);
 wav_fixture();u16(30,3);rejected(&w,SPEAKER_WAV_UNSUPPORTED);
 wav_fixture();u32(34,44100);rejected(&w,SPEAKER_WAV_UNSUPPORTED);
 wav_fixture();u16(44,8);rejected(&w,SPEAKER_WAV_UNSUPPORTED);
 wav_fixture();bytes[0]='X';rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();length--;rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();u32(50,8193);rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();u32(16,0xffffffff);rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();memcpy(bytes+22,"JUNK",4);rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();memcpy(bytes+46,"JUNK",4);rejected(&w,SPEAKER_WAV_INVALID);
 // Duplicate chunks and bounded scanning, including zero-length unknown chunks.
 wav_fixture();memcpy(bytes+length,"data",4);u32(length+4,0);length+=8;u32(4,length-8);rejected(&w,SPEAKER_WAV_INVALID);
 wav_fixture();for(unsigned i=0;i<129;i++){memcpy(bytes+length,"JUNK",4);u32(length+4,0);length+=8;}u32(4,length-8);rejected(&w,SPEAKER_WAV_INVALID);
 // Data before format is legal; parser seeks back after validating all chunks.
 wav_fixture();unsigned char saved[24];memcpy(saved,bytes+22,24);memmove(bytes+22,bytes+46,8200);memcpy(bytes+8222,saved,24);
 assert(speaker_wav_open(&w,"x")==SPEAKER_WAV_OK);assert(speaker_wav_read(&w,frames,2,1,&count)==SPEAKER_WAV_OK&&count==2);assert(frames[0]==0x7fff7fff&&frames[1]==0x80008000);speaker_wav_close(&w);
 wav_fixture();assert(speaker_wav_open(&w,"x")==SPEAKER_WAV_OK);assert(speaker_wav_read(&w,frames,2,0,&count)==SPEAKER_WAV_OK&&frames[0]==0&&frames[1]==0);
 assert(speaker_wav_read(&w,frames,513,1,&count)==SPEAKER_WAV_INVALID);length=position;assert(speaker_wav_read(&w,frames,512,1,&count)==SPEAKER_WAV_IO&&count==0);speaker_wav_close(&w);
 wav_fixture();assert(speaker_wav_open(&w,"x")==SPEAKER_WAV_OK);access=FS_ERROR_BUSY;assert(speaker_wav_read(&w,frames,512,1,&count)==SPEAKER_WAV_IO&&w.fs_error==FS_ERROR_BUSY);speaker_wav_close(&w);access=FS_OK;
 wav_fixture();length=54;u32(50,0);u32(4,length-8);assert(speaker_wav_open(&w,"x")==SPEAKER_WAV_OK);assert(speaker_wav_read(&w,frames,512,1,&count)==SPEAKER_WAV_OK&&count==0);speaker_wav_close(&w);assert(!handles);
 puts("PASS WAV: padding/reordered chunks, bounded streaming/scanning, mono duplication/gain/EOF, malformed/unsupported inputs and interrupted reads");
}
