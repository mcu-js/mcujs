#pragma once
/* SDK-shaped external fakes: production translation unit is unchanged. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT 0x107
#define pdPASS 1
#define pdMS_TO_TICKS(ms) ((ms)/10) /* deliberately 100 Hz to catch I2S tick bug */
#define GPIO_MODE_INPUT_OUTPUT 3
#define GPIO_PULLUP_ENABLE 1
#define I2C_NUM_1 1
#define I2C_MODE_MASTER 1
#define I2S_NUM_AUTO -1
#define I2S_ROLE_MASTER 0
#define I2S_DATA_BIT_WIDTH_32BIT 32
#define I2S_SLOT_MODE_STEREO 2
#define I2S_MCLK_MULTIPLE_256 256
#define I2S_GPIO_UNUSED -1
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
typedef pthread_mutex_t portMUX_TYPE;
#define portENTER_CRITICAL(p) pthread_mutex_lock(p)
#define portEXIT_CRITICAL(p) pthread_mutex_unlock(p)
typedef struct {uint64_t pin_bit_mask;int mode;} gpio_config_t;
typedef struct {int mode,sda_io_num,scl_io_num,sda_pullup_en,scl_pullup_en;struct {int clk_speed;} master;} i2c_config_t;
typedef struct {int id,role,dma_desc_num,dma_frame_num;} i2s_chan_config_t;
#define I2S_CHANNEL_DEFAULT_CONFIG(i,r) { .id=(i), .role=(r) }
typedef struct {int sample_rate_hz,mclk_multiple;} fake_clock;
typedef struct {int data_bit_width,slot_mode;} fake_slot;
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) { .sample_rate_hz=(rate) }
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(width,mode) { .data_bit_width=(width), .slot_mode=(mode) }
typedef struct {fake_clock clk_cfg;fake_slot slot_cfg;struct {int mclk,bclk,ws,dout,din;} gpio_cfg;} i2s_std_config_t;
typedef struct fake_rx *i2s_chan_handle_t;
typedef struct fake_timer *esp_timer_handle_t;
typedef struct {void (*callback)(void *);void *arg;const char *name;} esp_timer_create_args_t;
int gpio_set_level(int pin,int value);
int gpio_get_level(int pin);
int gpio_config(const gpio_config_t *config);
esp_err_t i2c_driver_install(int port,int mode,int rx,int tx,int flags);
esp_err_t i2c_param_config(int port,const i2c_config_t *config);
esp_err_t i2c_master_write_to_device(int port,int address,const uint8_t *data,size_t length,unsigned ticks);
esp_err_t i2c_driver_delete(int port);
esp_err_t i2s_new_channel(const i2s_chan_config_t *config,i2s_chan_handle_t *tx,i2s_chan_handle_t *rx);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t rx,const i2s_std_config_t *config);
esp_err_t i2s_channel_enable(i2s_chan_handle_t rx);
esp_err_t i2s_channel_read(i2s_chan_handle_t rx,void *data,size_t size,size_t *bytes,uint32_t timeout_ms);
esp_err_t i2s_channel_disable(i2s_chan_handle_t rx);
esp_err_t i2s_del_channel(i2s_chan_handle_t rx);
esp_err_t esp_timer_create(const esp_timer_create_args_t *config,esp_timer_handle_t *timer);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer,uint64_t us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
int64_t esp_timer_get_time(void);
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle);
void vTaskDelete(void *handle);
void vTaskDelay(unsigned ticks);
extern atomic_int fake_rail, fake_bus, fake_rx_count, fake_enabled, fake_tasks, fake_timers;
extern atomic_int fake_unmutes, fake_reads, fake_fault_at, fake_steps, fake_read_mode;
extern atomic_bool fake_fail_task, fake_hold_read, fake_in_read, fake_fail_mute, fake_fail_off, fake_fail_alloc;
extern atomic_llong fake_unmuted_at, fake_off_at;
void fake_reset(void);
void fake_wait_idle(void);
