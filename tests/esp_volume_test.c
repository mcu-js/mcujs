/* Production per-volume lease functions, SDK adapters replaced at their ABI. */
#include "fs.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
static bool app_host, device_task = true;
static unsigned exports, imports;
static fs_result_t media = FS_OK, export_result = FS_OK, import_result = FS_OK;
static bool is_device_task(void) { return device_task; }
fs_result_t fs_begin_host_access(void) { app_host = true; return FS_OK; }
fs_result_t fs_end_host_access(void) { app_host = false; return FS_OK; }
bool fs_host_owned(void) { return app_host; }
fs_result_t fs_msc_sync(void) { return app_host ? FS_OK : FS_ERROR_BUSY; }
uint32_t fs_get_total_sectors(void) { return 2048; }
fs_result_t fs_read_sector(uint32_t s,uint32_t o,void *b,uint32_t n) { (void)s;(void)o;(void)b;(void)n;return FS_OK; }
fs_result_t fs_write_sector(uint32_t s,uint32_t o,const void *b,uint32_t n) { return fs_read_sector(s,o,(void *)b,n); }
static fs_result_t sd_card_export(uint32_t *s,uint32_t *n) { exports++; *s=128;*n=4096;return export_result; }
static fs_result_t sd_card_import(uint32_t s,uint32_t n) { assert(s==128 && n==4096);imports++;return import_result; }
static fs_result_t sd_card_media_status(void) { return media; }
static fs_result_t sd_card_sync(void) { return media; }
static fs_result_t sd_card_transfer(uint32_t s,uint32_t n,uint32_t l,uint32_t o,void *b,uint32_t z,bool w) { assert(s==128 && n==4096);(void)l;(void)o;(void)b;(void)z;(void)w;return media; }
#include "esp-volume-functions.inc"
int main(void) {
 uint32_t n=99; unsigned char b[512];
 assert(fs_volume_capacity(1,&n)==FS_ERROR_BUSY && n==0);
 atomic_store(&s_sd_open_files,1);
 assert(fs_volume_begin_host_access(1)==FS_ERROR_BUSY && !exports);
 atomic_store(&s_sd_open_files,0);
 device_task=false; assert(fs_volume_begin_host_access(1)==FS_ERROR_BUSY);device_task=true;
 assert(fs_volume_begin_host_access(0)==FS_OK);
 assert(fs_volume_begin_host_access(1)==FS_OK && exports==1);
 assert(fs_volume_host_owned(0) && fs_volume_host_owned(1));
 assert(fs_volume_capacity(0,&n)==FS_OK && n==2048);
 assert(fs_volume_capacity(1,&n)==FS_OK && n==4096);
 assert(fs_volume_end_host_access(0)==FS_OK && fs_volume_host_owned(1));
 assert(fs_volume_read_sector(1,0,0,b,512)==FS_OK);
 assert(fs_volume_end_host_access(1)==FS_OK && imports==1);
 assert(fs_volume_msc_status(1)==FS_ERROR_BUSY);
 assert(fs_volume_begin_host_access(1)==FS_OK);
 media=FS_ERROR_IO;
 assert(fs_volume_msc_sync(1)==FS_ERROR_IO);
 assert(fs_volume_capacity(1,&n)==FS_ERROR_IO && n==0);
 import_result=FS_ERROR_IO;
 assert(fs_volume_end_host_access(1)==FS_ERROR_IO);
 assert(!fs_volume_host_owned(1));
 assert(fs_volume_begin_host_access(1)==FS_ERROR_IO);
 assert(fs_volume_begin_host_access(0)==FS_OK);
 assert(fs_volume_begin_host_access(2)==FS_ERROR_INVALID);
 puts("PASS ESP independent volume leases, open-handle/task fencing, geometry, remount fault");
}
