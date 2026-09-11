/*
 * Bounded, explicitly app-started ES8311 input for Waveshare ePaper V2 only.
 * No SDK objects, powered codec, task, or DMA exist merely because open() ran.
 *
 * Register values/sequencing adapted from Espressif's Apache-2.0 ES8311 driver:
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Primary source: waveshareteam/ESP32-S3-ePaper-1.54 @
 * 9957d0f4fc7cd40d1d42880cb1b74a8d6782a6c2,
 * 02_Example/Arduino/08_Audio_Test/src/esp_codec_dev/device/es8311/es8311.c
 * This is an input-only subset, NOT the vendor driver or codec framework.
 * Fixed 12 dB PGA (REG16=2), 0 dB ADC volume (REG17=0xbf), analog mic.
 * See docs/docs/microphone.md for the bounded public capture contract.
 */
#include "jerryscript.h"
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if !defined(MCUJS_BOARD_WAVESHARE_ESP32S3_EPAPER_1_54_V2)
#error "microphone.c is only supported on Waveshare ESP32-S3 ePaper 1.54 V2"
#endif

#define AUDIO_RAIL 42
#define AUDIO_PA 46
#define CONTROL_PORT I2C_NUM_1
#define CONTROL_ADDRESS 0x18
#define READ_MS 10
#define INIT_US 200000
#define JOIN_US 2000000
#define MAX_BYTES 32000

/* All Jerry/API operations run on the engine task, except cleanup(). Cleanup
 * first cuts power without taking the API gate, then joins cooperatively.
 * SDK handles are worker-local: nobody ever deletes/suspends another task.
 * done's release/acquire handoff protects pcm, used, error and operation.
 */
static atomic_flag api_gate = ATOMIC_FLAG_INIT;
static portMUX_TYPE rail_gate = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool done = ATOMIC_VAR_INIT(true);
static atomic_bool cancelled, expired, shutdown_requested, privacy_fault, pins_ready;
static uint32_t owner, next_owner, generation;
static bool operation;
static uint8_t *pcm;
static size_t used, capacity;
static int duration_ms;
static const char *operation_error;
/* Below fields only under rail_gate. */
static bool guard_active;
static int64_t deadline;

static void api_lock(void) {
    while (atomic_flag_test_and_set_explicit(&api_gate, memory_order_acquire)) vTaskDelay(1);
}
static void api_unlock(void) { atomic_flag_clear_explicit(&api_gate, memory_order_release); }

/* GPIO write acknowledgement plus input readback; rail configured INPUT_OUTPUT
 * so readback is meaningful. Failure is sticky, never represented as safe idle.
 * No I2C, I2S, allocation or waiting in this short critical section.
 */
static bool rail_off_locked(void) {
    bool ok = gpio_set_level(AUDIO_RAIL, 1) == ESP_OK;
    ok = (gpio_set_level(AUDIO_PA, 0) == ESP_OK) && ok;
    if (atomic_load(&pins_ready))
        ok = gpio_get_level(AUDIO_RAIL) == 1 && gpio_get_level(AUDIO_PA) == 0 && ok;
    if (!ok) atomic_store(&privacy_fault, true);
    return ok;
}
static bool cut_power(bool cancel) {
    portENTER_CRITICAL(&rail_gate);
    if (cancel) atomic_store(&cancelled, true);
    bool ok = rail_off_locked();
    portEXIT_CRITICAL(&rail_gate);
    return ok;
}
static bool interrupted(void) {
    return atomic_load(&cancelled) || atomic_load(&expired) || atomic_load(&shutdown_requested);
}
static void deadline_guard(void *arg) {
    portENTER_CRITICAL(&rail_gate);
    if (guard_active && generation == (uint32_t)(uintptr_t)arg && esp_timer_get_time() >= deadline) {
        atomic_store(&expired, true);
        (void)rail_off_locked();
    }
    portEXIT_CRITICAL(&rail_gate);
}
static esp_err_t codec_write(uint8_t reg, uint8_t value) {
    uint8_t bytes[] = {reg, value};
    /* Legacy I2C takes ticks; unlike the I2S channel API below. */
    return i2c_master_write_to_device(CONTROL_PORT, CONTROL_ADDRESS, bytes, sizeof(bytes),
                                    pdMS_TO_TICKS(20) ? pdMS_TO_TICKS(20) : 1);
}
static bool codec_init_muted(void) {
    /* First ADC write is mute, repeated after reset. DAC input is disabled,
     * DAC volume zero, and GPIO45 is never routed to an I2S transmitter.
     * 4.096 MHz MCLK / 256 = 16 kHz; BCLK = MCLK/4 = 64 Fs.
     */
    static const uint8_t registers[][2] = {
        {0x17,0x00}, {0x44,0x08}, {0x44,0x08},
        {0x01,0x30}, {0x02,0x00}, {0x03,0x10}, {0x04,0x20}, {0x05,0x00},
        {0x0b,0x00}, {0x0c,0x00}, {0x10,0x1f}, {0x11,0x7f},
        {0x00,0x80}, {0x17,0x00}, {0x01,0x3f},
        {0x06,0x03}, {0x07,0x00}, {0x08,0xff},
        {0x09,0x4c}, {0x0a,0x0c}, {0x32,0x00},
        {0x13,0x10}, {0x1b,0x0a}, {0x1c,0x6a},
        {0x0e,0x02}, {0x12,0x02}, {0x14,0x1a}, {0x0d,0x01},
        {0x15,0x40}, {0x16,0x02}, {0x45,0x00}
    };
    for (size_t i=0; i<sizeof(registers)/sizeof(registers[0]); ++i) {
        if (interrupted() || codec_write(registers[i][0], registers[i][1]) != ESP_OK) return false;
    }
    return !interrupted();
}

static void capture_worker(void *arg) {
    i2s_chan_handle_t rx = NULL;
    esp_timer_handle_t timer = NULL;
    bool bus = false, rx_enabled = false, powered = false;
    const char *error = "EIO";
    uint32_t raw[256]; /* 128 stereo frames, 1024-byte fixed scratch. */
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4;
    channel.dma_frame_num = 64;
    i2s_std_config_t format = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk=14, .bclk=15, .ws=38, .dout=I2S_GPIO_UNUSED, .din=16}
    };
    format.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    i2c_config_t control = {.mode=I2C_MODE_MASTER, .sda_io_num=47, .scl_io_num=48,
        .sda_pullup_en=GPIO_PULLUP_ENABLE, .scl_pullup_en=GPIO_PULLUP_ENABLE,
        .master.clk_speed=100000};
    esp_timer_create_args_t guard = {.callback=deadline_guard, .arg=arg, .name="mic-limit"};

    if (interrupted()) goto finish;
    /* Claim before changing config: never reconfigure another I2C owner's bus. */
    if (i2c_driver_install(CONTROL_PORT, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) {
        error="ERR_RESOURCE_EXHAUSTED"; goto finish;
    }
    bus=true;
    if (i2c_param_config(CONTROL_PORT, &control) != ESP_OK) goto finish;
    if (i2s_new_channel(&channel, NULL, &rx) != ESP_OK) {
        error="ERR_RESOURCE_EXHAUSTED"; goto finish;
    }
    if (i2s_channel_init_std_mode(rx, &format) != ESP_OK) goto finish;
    if (esp_timer_create(&guard, &timer) != ESP_OK) { error="ERR_RESOURCE_EXHAUSTED"; goto finish; }
    portENTER_CRITICAL(&rail_gate);
    deadline = esp_timer_get_time() + INIT_US;
    guard_active = true;
    portEXIT_CRITICAL(&rail_gate);
    if (esp_timer_start_periodic(timer, 1000) != ESP_OK) { error="ERR_RESOURCE_EXHAUSTED"; goto finish; }

    portENTER_CRITICAL(&rail_gate);
    if (!interrupted() && !atomic_load(&privacy_fault)) {
        powered = gpio_set_level(AUDIO_RAIL, 0) == ESP_OK && gpio_get_level(AUDIO_RAIL) == 0;
    }
    portEXIT_CRITICAL(&rail_gate);
    if (!powered) goto finish;
    if (!codec_init_muted()) goto finish;
    if (interrupted() || i2s_channel_enable(rx) != ESP_OK) goto finish;
    rx_enabled=true;
    /* DMA only exists inside this admitted operation. No pre-capture drain.
     * Establish capture deadline before the single checked unmute transaction.
     */
    portENTER_CRITICAL(&rail_gate);
    if (!interrupted()) deadline=esp_timer_get_time() + (int64_t)duration_ms*1000;
    portEXIT_CRITICAL(&rail_gate);
    if (interrupted() || codec_write(0x17, 0xbf) != ESP_OK || interrupted()) goto finish;

    while (used < capacity && !interrupted()) {
        int64_t now=esp_timer_get_time(), remaining;
        portENTER_CRITICAL(&rail_gate);
        remaining=deadline-now;
        portEXIT_CRITICAL(&rail_gate);
        if (remaining <= 0) { atomic_store(&expired,true); break; }
        uint32_t timeout_ms=(uint32_t)((remaining+999)/1000);
        if (timeout_ms > READ_MS) timeout_ms=READ_MS;
        size_t frames=(capacity-used)/2;
        if (frames > 128) frames=128;
        size_t bytes=0;
        esp_err_t status=i2s_channel_read(rx, raw, frames*8, &bytes, timeout_ms);
        /* I2S timeout is MILLISECONDS, NOT FreeRTOS ticks. Status first. */
        if (status != ESP_OK && status != ESP_ERR_TIMEOUT) goto finish;
        if (bytes > frames*8 || bytes%8) goto finish;
        if (atomic_load(&cancelled) || atomic_load(&shutdown_requested)) goto finish;
        for (size_t i=0; i<bytes/8; ++i) {
            /* ES8311 16-bit left channel in 32-bit I2S slots. Explicit LE. */
            uint16_t sample=(uint16_t)(raw[i*2] >> 16);
            pcm[used++]=(uint8_t)sample; pcm[used++]=(uint8_t)(sample >> 8);
        }
        if (!bytes) vTaskDelay(1); /* Missing input cannot busy-spin forever. */
    }
    /* A wall-clock cutoff may yield a shorter recording; never pad with fake
     * silence. Empty input is an error, not a successful empty recording. */
    if (used && !atomic_load(&cancelled) && !atomic_load(&shutdown_requested)) error=NULL;
finish:
    /* Power first: even a wedged/failed I2C mute cannot delay rail shutdown.
     * Always issue a real mute too; never infer it from cached register state.
     */
    (void)cut_power(false);
    if (powered && bus) (void)codec_write(0x17,0x00);
    if (rx_enabled && i2s_channel_disable(rx) != ESP_OK) error="EIO";
    if (rx && i2s_del_channel(rx) != ESP_OK) { atomic_store(&privacy_fault,true); error="EIO"; }
    if (bus && i2c_driver_delete(CONTROL_PORT) != ESP_OK) { atomic_store(&privacy_fault,true); error="EIO"; }
    portENTER_CRITICAL(&rail_gate);
    guard_active=false;
    portEXIT_CRITICAL(&rail_gate);
    if (timer) {
        (void)esp_timer_stop(timer);
        if (esp_timer_delete(timer) != ESP_OK) { atomic_store(&privacy_fault,true); error="EIO"; }
    }
    if (atomic_load(&privacy_fault)) error="ERR_MICROPHONE_PRIVACY";
    if (atomic_load(&cancelled) || atomic_load(&shutdown_requested)) error="ECANCELED";
    if (error) { free(pcm); pcm=NULL; used=0; }
    operation_error=error;
    atomic_store_explicit(&done,true,memory_order_release);
    /* Nothing may access operation state after publishing done. */
    vTaskDelete(NULL);
}

static jerry_value_t fail(const char *code) {
    jerry_value_t e=jerry_error_sz(JERRY_ERROR_COMMON,code);
    jerry_value_t c=jerry_string_sz(code);
    jerry_value_free(jerry_object_set_sz(e,"code",c)); jerry_value_free(c);
    return jerry_throw_value(e,true);
}
static bool valid_token(const jerry_value_t args[], jerry_length_t argc) {
    return argc && owner && jerry_value_is_number(args[0]) && jerry_value_as_number(args[0]) == owner;
}
static bool join_and_discard(void) {
    (void)cut_power(true);
    int64_t limit=esp_timer_get_time()+JOIN_US;
    while (!atomic_load_explicit(&done,memory_order_acquire)) {
        if (esp_timer_get_time() >= limit) return false; /* retain, NEVER free a live worker */
        vTaskDelay(1);
    }
    free(pcm); pcm=NULL; used=capacity=0; operation=false; operation_error=NULL;
    return true;
}
#define HANDLER(name) static jerry_value_t name(const jerry_call_info_t *info, const jerry_value_t args[], const jerry_length_t argc)
HANDLER(native_open) {
    (void)info;(void)args;(void)argc;
    api_lock();
    const char *error=NULL;
    if (atomic_load(&privacy_fault)) error="ERR_MICROPHONE_PRIVACY";
    else if (atomic_load(&shutdown_requested)) error="ENXIO";
    else if (owner) error="EBUSY";
    else if (next_owner == INT32_MAX) error="ERR_RESOURCE_EXHAUSTED";
    else owner=++next_owner;
    jerry_value_t result=error?fail(error):jerry_number(owner);
    api_unlock();return result;
}
HANDLER(native_start) {
    (void)info;api_lock(); const char *error=NULL;
    double ms=argc>1 && jerry_value_is_number(args[1])?jerry_value_as_number(args[1]):NAN;
    if (!valid_token(args,argc)) error="ENXIO";
    else if (!isfinite(ms) || floor(ms)!=ms || ms<20 || ms>1000) error="ERR_OUT_OF_RANGE";
    else if (operation) error="EBUSY";
    else if (atomic_load(&privacy_fault)) error="ERR_MICROPHONE_PRIVACY";
    else if (atomic_load(&shutdown_requested)) error="ENXIO";
    else if (generation == UINT32_MAX) error="ERR_RESOURCE_EXHAUSTED";
    else {
        /* Preload before output enable. Startup/open never turns on the rail. */
        gpio_config_t pins={.pin_bit_mask=(1ULL<<AUDIO_RAIL)|(1ULL<<AUDIO_PA), .mode=GPIO_MODE_INPUT_OUTPUT};
        bool pins_ok=gpio_set_level(AUDIO_RAIL,1)==ESP_OK;
        pins_ok=(gpio_set_level(AUDIO_PA,0)==ESP_OK) && pins_ok;
        pins_ok=(gpio_config(&pins)==ESP_OK) && pins_ok;
        atomic_store(&pins_ready,pins_ok);
        if (!cut_power(false) || !pins_ok) {
            atomic_store(&privacy_fault,true);error="ERR_MICROPHONE_PRIVACY";
        }
        else {
            duration_ms=(int)ms; capacity=(size_t)duration_ms*32;
            pcm=malloc(capacity);
            if (!pcm) error="ERR_RESOURCE_EXHAUSTED";
            else {
                used=0; operation_error=NULL; operation=true;
                portENTER_CRITICAL(&rail_gate);
                ++generation;
                atomic_store(&cancelled,false); atomic_store(&expired,false);
                /* Cleanup may have published shutdown while start owned API gate. */
                bool stop=atomic_load(&shutdown_requested);
                portEXIT_CRITICAL(&rail_gate);
                atomic_store_explicit(&done,false,memory_order_release);
                if (stop || xTaskCreate(capture_worker,"microphone",4096,(void *)(uintptr_t)generation,5,NULL)!=pdPASS) {
                    atomic_store(&done,true); free(pcm);pcm=NULL;operation=false;
                    error=stop?"ENXIO":"ERR_RESOURCE_EXHAUSTED";
                    (void)cut_power(false);
                }
            }
        }
    }
    jerry_value_t result=error?fail(error):jerry_undefined();api_unlock();return result;
}
HANDLER(native_poll) {
    (void)info;api_lock();jerry_value_t result;
    if (!valid_token(args,argc)) result=fail("ENXIO");
    else if (!operation) result=fail("EINVAL");
    else if (!atomic_load_explicit(&done,memory_order_acquire)) result=jerry_number(1);
    else if (operation_error) result=fail(operation_error);
    else result=jerry_number(2);
    api_unlock();return result;
}
HANDLER(native_result) {
    (void)info;api_lock();jerry_value_t result;
    if (!valid_token(args,argc)) result=fail("ENXIO");
    else if (!operation) result=fail("EINVAL");
    else if (!atomic_load_explicit(&done,memory_order_acquire)) result=fail("EBUSY");
    else if (operation_error) result=fail(operation_error);
    else {
        result=jerry_typedarray(JERRY_TYPEDARRAY_UINT8,(jerry_length_t)used);
        if (!jerry_value_is_exception(result)) {
            jerry_length_t offset,length;
            jerry_value_t buffer=jerry_typedarray_buffer(result,&offset,&length);
            if (jerry_arraybuffer_write(buffer,offset,pcm,(jerry_length_t)used)!=used) {
                jerry_value_free(result);result=fail("ERR_RESOURCE_EXHAUSTED");
            }
            jerry_value_free(buffer);
        }
        free(pcm);pcm=NULL;used=capacity=0;operation=false;
    }
    api_unlock();return result;
}
static jerry_value_t end_owner(const jerry_value_t args[], jerry_length_t argc,bool release) {
    api_lock();const char *error=NULL;
    if (!valid_token(args,argc)) error="ENXIO";
    else if (!join_and_discard()) error="EBUSY";
    else { if (release) owner=0; if (atomic_load(&privacy_fault)) error="ERR_MICROPHONE_PRIVACY"; }
    jerry_value_t result=error?fail(error):jerry_undefined();api_unlock();return result;
}
HANDLER(native_stop) { (void)info;return end_owner(args,argc,false); }
HANDLER(native_close) { (void)info;return end_owner(args,argc,true); }
HANDLER(native_state) {
    (void)info;(void)args;(void)argc;api_lock();
    jerry_value_t result=jerry_string_sz(owner || operation || atomic_load(&privacy_fault)?"busy":"idle");
    api_unlock();return result;
}
void js_microphone_cleanup(void) {
    atomic_store(&shutdown_requested,true);
    (void)cut_power(true); /* Before gate/join, never behind any SDK locks. */
    api_lock();
    if (join_and_discard()) owner=0;
    api_unlock();
}
jerry_value_t js_create_microphone_native_module(void) {
    /* Safe to construct after engine cleanup, but never clear a privacy fault. */
    api_lock();
    if (!owner && !operation) atomic_store(&shutdown_requested,false);
    api_unlock();
    jerry_value_t module=jerry_object();
    const struct {const char *name;jerry_external_handler_t handler;} methods[]={
        {"open",native_open},{"start",native_start},{"poll",native_poll},{"result",native_result},
        {"stop",native_stop},{"close",native_close},{"state",native_state}
    };
    for (size_t i=0;i<sizeof(methods)/sizeof(methods[0]);++i) {
        jerry_value_t fn=jerry_function_external(methods[i].handler);
        jerry_value_free(jerry_object_set_sz(module,methods[i].name,fn));jerry_value_free(fn);
    }
    return module;
}
