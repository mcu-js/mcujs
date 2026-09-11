#include "microphone_native_sdk.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
atomic_int fake_rail=1,fake_bus,fake_rx_count,fake_enabled,fake_tasks,fake_timers;
atomic_int fake_unmutes,fake_reads,fake_fault_at,fake_steps,fake_read_mode;
atomic_bool fake_fail_task,fake_hold_read,fake_in_read,fake_fail_mute,fake_fail_off,fake_fail_alloc;
atomic_llong fake_unmuted_at,fake_off_at;
static bool muted=true;
static bool fails(void) { return atomic_fetch_add(&fake_steps,1)+1==atomic_load(&fake_fault_at); }
int64_t esp_timer_get_time(void) {
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
void * __real_malloc(size_t size);
void * __wrap_malloc(size_t size) {
    if (size==640 && atomic_exchange(&fake_fail_alloc,false)) return NULL;
    return __real_malloc(size);
}
int gpio_set_level(int pin,int level) {
    assert(pin==42 || pin==46);
    if (pin==46) {assert(!level);return ESP_OK;}
    if (level && atomic_load(&fake_fail_off)) return ESP_FAIL;
    if (level && atomic_exchange(&fake_rail,1)==0) atomic_store(&fake_off_at,esp_timer_get_time());
    if (!level) { assert(atomic_load(&fake_bus)&&atomic_load(&fake_rx_count)&&atomic_load(&fake_timers));atomic_store(&fake_rail,0);muted=true; }
    return ESP_OK;
}
int gpio_get_level(int pin) {assert(pin==42||pin==46);return pin==42?atomic_load(&fake_rail):0;}
int gpio_config(const gpio_config_t *c) {assert(c->pin_bit_mask==((1ULL<<42)|(1ULL<<46))&&c->mode==GPIO_MODE_INPUT_OUTPUT);return ESP_OK;}
esp_err_t i2c_driver_install(int port,int mode,int rx,int tx,int flags) {
    assert(port==1&&mode==1&&!rx&&!tx&&!flags&&!atomic_load(&fake_bus));if(fails())return ESP_FAIL;

    atomic_store(&fake_bus,1);return ESP_OK;
}
esp_err_t i2c_param_config(int port,const i2c_config_t *c) {
    assert(port==1&&atomic_load(&fake_bus)&&c->sda_io_num==47&&c->scl_io_num==48&&c->master.clk_speed==100000);
    return fails()?ESP_FAIL:ESP_OK;
}
esp_err_t i2c_master_write_to_device(int port,int address,const uint8_t *data,size_t size,unsigned ticks) {
    assert(port==1&&address==0x18&&size==2&&ticks==2&&atomic_load(&fake_bus));
    if (data[0]==0x17) {
        if(data[1]) {
            assert(data[1]==0xbf&&muted&&atomic_load(&fake_enabled)&&!atomic_load(&fake_rail));
            atomic_fetch_add(&fake_unmutes,1);atomic_store(&fake_unmuted_at,esp_timer_get_time());muted=false;
        } else {
            muted=true;
            if(atomic_load(&fake_fail_mute)&&atomic_load(&fake_unmutes)) return ESP_FAIL;
        }
    }
    /* Fault may occur AFTER hardware accepted the write: unknown codec state. */
    return fails()?ESP_FAIL:ESP_OK;
}
esp_err_t i2c_driver_delete(int port) {assert(port==1&&atomic_load(&fake_bus)&&atomic_load(&fake_rail));atomic_store(&fake_bus,0);return ESP_OK;}
struct fake_rx {bool initialized;};
esp_err_t i2s_new_channel(const i2s_chan_config_t *c,i2s_chan_handle_t *tx,i2s_chan_handle_t *rx) {
    assert(!tx&&rx&&c->id==-1&&c->dma_desc_num==4&&c->dma_frame_num==64);
    if(fails())return ESP_FAIL;
    *rx=calloc(1,sizeof(**rx));assert(*rx);atomic_fetch_add(&fake_rx_count,1);return ESP_OK;
}
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t rx,const i2s_std_config_t *c) {
    assert(rx&&c->clk_cfg.sample_rate_hz==16000&&c->clk_cfg.mclk_multiple==256);
    assert(c->slot_cfg.data_bit_width==32&&c->slot_cfg.slot_mode==2);
    assert(c->gpio_cfg.mclk==14&&c->gpio_cfg.bclk==15&&c->gpio_cfg.ws==38&&c->gpio_cfg.din==16&&c->gpio_cfg.dout==-1);
    if(fails())return ESP_FAIL;
    rx->initialized=true;return ESP_OK;
}
esp_err_t i2s_channel_enable(i2s_chan_handle_t rx) {assert(rx&&rx->initialized&&muted);if(fails())return ESP_FAIL;
    atomic_store(&fake_enabled,1);return ESP_OK;}
esp_err_t i2s_channel_read(i2s_chan_handle_t rx,void *dst,size_t size,size_t *bytes,uint32_t timeout_ms) {
    assert(rx&&atomic_load(&fake_enabled)&&size&&size<=1024&&size%8==0&&timeout_ms>=1&&timeout_ms<=10);
    atomic_store(&fake_in_read,true);atomic_fetch_add(&fake_reads,1);
    while(atomic_load(&fake_hold_read)&&!atomic_load(&fake_rail)) usleep(1000);
    /* The fake 100Hz RTOS would give 1 if production pre-converted READ_MS. */
    if(atomic_load(&fake_read_mode)!=4) assert(timeout_ms>=2);
    int mode=atomic_load(&fake_read_mode);
    *bytes=0;
    if(mode==1) {usleep(timeout_ms*1000);atomic_store(&fake_in_read,false);return ESP_ERR_TIMEOUT;}
    if(mode==2) {atomic_store(&fake_in_read,false);return ESP_FAIL;}
    if(mode==3) {*bytes=3;atomic_store(&fake_in_read,false);return ESP_OK;}
    if(mode==4) {usleep(timeout_ms*1000);atomic_store(&fake_in_read,false);return ESP_OK;}
    /* Explicit fixture: left=-1234, right=0x1234; right MUST NOT be returned. */
    uint32_t *data=dst;
    for(size_t i=0;i<size/8;i++){data[i*2]=0xfb2e0000;data[i*2+1]=0x12340000;}
    *bytes=size;usleep(1000);atomic_store(&fake_in_read,false);return ESP_OK;
}
esp_err_t i2s_channel_disable(i2s_chan_handle_t rx) {assert(rx&&!atomic_load(&fake_in_read)&&atomic_load(&fake_rail));atomic_store(&fake_enabled,0);return ESP_OK;}
esp_err_t i2s_del_channel(i2s_chan_handle_t rx) {assert(rx&&!atomic_load(&fake_enabled)&&!atomic_load(&fake_in_read));free(rx);atomic_fetch_sub(&fake_rx_count,1);return ESP_OK;}
struct fake_timer {esp_timer_create_args_t c;pthread_t thread;atomic_bool stopped;bool started;};
static void *timer_thread(void *p) {
    struct fake_timer *t=p;
    while(!atomic_load(&t->stopped)){usleep(1000);if(!atomic_load(&t->stopped))t->c.callback(t->c.arg);}
    return NULL;
}
esp_err_t esp_timer_create(const esp_timer_create_args_t *c,esp_timer_handle_t *t) {
    if(fails())return ESP_FAIL;
    *t=calloc(1,sizeof(**t));assert(*t);(*t)->c=*c;atomic_fetch_add(&fake_timers,1);return ESP_OK;
}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t,uint64_t us) {
    assert(t&&us==1000);if(fails())return ESP_FAIL;
    t->started=true;assert(!pthread_create(&t->thread,NULL,timer_thread,t));return ESP_OK;
}
esp_err_t esp_timer_stop(esp_timer_handle_t t) {
    atomic_store(&t->stopped,true);if(t->started){pthread_join(t->thread,NULL);t->started=false;}return ESP_OK;
}
esp_err_t esp_timer_delete(esp_timer_handle_t t) {assert(!t->started);free(t);atomic_fetch_sub(&fake_timers,1);return ESP_OK;}
struct task_args {void (*entry)(void *);void *arg;};
static void *task_thread(void *p) {struct task_args a=*(struct task_args *)p;free(p);a.entry(a.arg);abort();}
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    assert(entry&&name&&stack==4096&&priority==5&&!handle);
    if(atomic_load(&fake_fail_task))return 0;
    struct task_args *p=malloc(sizeof(*p));assert(p);p->entry=entry;p->arg=arg;
    pthread_t thread;atomic_fetch_add(&fake_tasks,1);assert(!pthread_create(&thread,NULL,task_thread,p));pthread_detach(thread);return pdPASS;
}
void vTaskDelete(void *handle) {assert(!handle);atomic_fetch_sub(&fake_tasks,1);pthread_exit(NULL);}
void vTaskDelay(unsigned ticks) {assert(ticks);usleep(ticks*10000);}
void fake_wait_idle(void) {
    int64_t end=esp_timer_get_time()+2500000;
    while(atomic_load(&fake_tasks)){assert(esp_timer_get_time()<end);usleep(1000);}
    assert(atomic_load(&fake_rail)&&!atomic_load(&fake_bus)&&!atomic_load(&fake_rx_count)&&!atomic_load(&fake_enabled)&&!atomic_load(&fake_timers));
}
void fake_reset(void) {
    fake_wait_idle();atomic_store(&fake_fault_at,0);atomic_store(&fake_steps,0);
    atomic_store(&fake_unmutes,0);atomic_store(&fake_reads,0);atomic_store(&fake_read_mode,0);
    atomic_store(&fake_fail_task,false);atomic_store(&fake_fail_alloc,false);atomic_store(&fake_hold_read,false);
    atomic_store(&fake_fail_mute,false);atomic_store(&fake_unmuted_at,0);atomic_store(&fake_off_at,0);
}
