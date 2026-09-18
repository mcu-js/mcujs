/* Actual IDF FatFs and sd_card.c; only SDK transport and VFS allocation faked. */
#include "sd_card.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SECTORS 8192
static unsigned char media[SECTORS][512];
static const ff_diskio_impl_t *disk;
static FATFS volume, app_volume;
static unsigned char app_media[SECTORS][512];
static unsigned writes, reads, inits, vfs_live;
static int fail_read, fail_write, fail_sync;
const PARTITION VolToPart[FF_VOLUMES]={{0,0},{1,0},{2,0}};
DWORD get_fattime(void) { return (2026u-1980u)<<25 | 9u<<21 | 17u<<16; }
int ff_mutex_create(int v) { (void)v; return 1; }
void ff_mutex_delete(int v) { (void)v; }
int ff_mutex_take(int v) { (void)v; return 1; }
void ff_mutex_give(int v) { (void)v; }
void *ff_memalloc(unsigned n) { return malloc(n); }
void ff_memfree(void *p) { free(p); }
void vTaskDelay(unsigned ms) { (void)ms; }
esp_err_t sdmmc_host_init(void) { inits++; return ESP_OK; }
esp_err_t sdmmc_host_init_slot(int n, const sdmmc_slot_config_t *s) {
    assert(n==1 && s->width==1 && s->clk==39 && s->cmd==41 && s->d0==40 && s->cd==-1); return ESP_OK;
}
esp_err_t sdmmc_card_init(const sdmmc_host_t *h, sdmmc_card_t *c) {
    assert(h->flags==SDMMC_HOST_FLAG_1BIT && h->max_freq_khz==4000);
    c->csd.capacity=SECTORS; c->csd.sector_size=512; return ESP_OK;
}
esp_err_t sdmmc_read_sectors(sdmmc_card_t *c, void *b, size_t s, size_t n) {
    assert(c->csd.capacity==SECTORS && s<SECTORS && n && n<=SECTORS-s); reads++;
    if(fail_read)return -1; memcpy(b,media[s],n*512); return ESP_OK;
}
esp_err_t sdmmc_write_sectors(sdmmc_card_t *c, const void *b, size_t s, size_t n) {
    assert(c->csd.capacity==SECTORS && s<SECTORS && n && n<=SECTORS-s); writes++;
    if(fail_write)return -1; memcpy(media[s],b,n*512); return ESP_OK;
}
esp_err_t sdmmc_get_status(sdmmc_card_t *c) { assert(c->csd.capacity==SECTORS); return fail_sync?-1:ESP_OK; }
esp_err_t ff_diskio_get_drive(BYTE *d) { assert(!disk); *d=1; return ESP_OK; }
void ff_diskio_register(BYTE d, const ff_diskio_impl_t *impl) { assert(d==1); disk=impl; }
DSTATUS disk_initialize(BYTE d) { return d == 0 ? 0 : disk->init(d); }
DSTATUS disk_status(BYTE d) { return d == 0 ? 0 : disk->status(d); }
DRESULT disk_read(BYTE d,BYTE *b,LBA_t s,UINT n) { if (d == 0) { assert(s < SECTORS && n <= SECTORS-s); memcpy(b,app_media[s],n*512); return RES_OK; } return disk->read(d,b,s,n); }
DRESULT disk_write(BYTE d,const BYTE *b,LBA_t s,UINT n) { if (d == 0) { assert(s < SECTORS && n <= SECTORS-s); memcpy(app_media[s],b,n*512); return RES_OK; } return disk->write(d,b,s,n); }
DRESULT disk_ioctl(BYTE d,BYTE cmd,void *b) { if (d == 0) { if (cmd == GET_SECTOR_SIZE) { *(WORD *)b=512; return RES_OK; } return cmd == CTRL_SYNC ? RES_OK : RES_PARERR; } return disk->ioctl(d,cmd,b); }
esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t *c,FATFS **out) {
    assert(!vfs_live && !strcmp(c->fat_drive,"1:") && !strcmp(c->base_path,SD_CARD_BASE_PATH));
    vfs_live=1; *out=&volume; return ESP_OK;
}
esp_err_t esp_vfs_fat_unregister_path(const char *p) {
    assert(vfs_live && !strcmp(p,SD_CARD_BASE_PATH)); vfs_live=0; return ESP_OK;
}
static void le16(unsigned char *p,unsigned n) {p[0]=n;p[1]=n>>8;}
static void le32(unsigned char *p,unsigned n) {le16(p,n);le16(p+2,n>>16);}
/* Independent FAT16 fixture with 7 trailing padding sectors, MBR optional. */
static void seed(unsigned start) {
    unsigned char *b=media[start]; b[0]=0xeb; b[2]=0x90; le16(b+11,512); b[13]=1;
    le16(b+14,1); b[16]=2; le16(b+17,512); le16(b+19,8192-start);
    le16(b+22,32); le16(b+510,0xaa55);
    le16(media[start+1],0xfff8); le16(media[start+1]+2,0xffff);
    memcpy(media[start+33],media[start+1],512);
    if(start) {le16(media[0]+510,0xaa55);media[0][450]=6;le32(media[0]+454,start);le32(media[0]+458,8192-start);}
}
int main(int argc,char **argv) {
    assert(argc==2); unsigned start=!strcmp(argv[1],"mbr")?128:0; seed(start);
    memcpy(app_media, media, sizeof(media));
    assert(f_mount(&app_volume,"0:",1)==FR_OK);
    FIL app_file; UINT app_n;
    assert(f_open(&app_file,"0:/APP.TXT",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    assert(f_write(&app_file,"app intact",10,&app_n)==FR_OK && app_n==10);
    assert(f_close(&app_file)==FR_OK);
    if(!strcmp(argv[1],"malformed")) {le16(media[0]+19,9000);}
    uint32_t base=99,count=99;
    fs_result_t r=sd_card_export(&base,&count);
    if(!strcmp(argv[1],"malformed")) {assert(r==FS_ERROR_UNSUPPORTED && !writes && !vfs_live);return 0;}
    assert(r==FS_OK && base==start && count==8192-start && !vfs_live && !writes);
    unsigned char b[512]={0};
    assert(sd_card_transfer(base,count,count,0,b,1,false)==FS_ERROR_INVALID);
    assert(sd_card_transfer(base,count,0,511,b,2,true)==FS_ERROR_INVALID);
    assert(sd_card_transfer(base,count,0,0,NULL,1,true)==FS_ERROR_INVALID);
    assert(sd_card_transfer(base,count,count-1,511,(unsigned char[]){0xa5},1,true)==FS_OK);
    assert(media[8191][511]==0xa5);
    assert(sd_card_import(base,count)==FS_OK && vfs_live);
    FIL file; UINT n;
    assert(f_open(&file,"1:/NATIVE.TXT",FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    assert(f_write(&file,"native rw",9,&n)==FR_OK && n==9);
    assert(f_close(&file)==FR_OK);
    assert(sd_card_export(&base,&count)==FS_OK && !vfs_live && inits==1);
    assert(sd_card_import(base,count)==FS_OK);
    assert(f_open(&file,"1:/NATIVE.TXT",FA_READ)==FR_OK);
    assert(f_read(&file,b,sizeof(b),&n)==FR_OK && n==9 && !memcmp(b,"native rw",9));
    assert(f_close(&file)==FR_OK);
    assert(f_open(&app_file,"0:/APP.TXT",FA_READ)==FR_OK);
    assert(f_read(&app_file,b,sizeof(b),&app_n)==FR_OK && app_n==10 && !memcmp(b,"app intact",10));
    assert(f_close(&app_file)==FR_OK);
    printf("PASS ESP IDF FatFs native RW/export/import %s (%u reads, %u writes)\n",argv[1],reads,writes);
}
