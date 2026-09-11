/* Configured Waveshare RP2350 Touch LCD 2.8 output, never a generic I2S API. */
#include "bindings.h"
#include "validation.h"
#include "speaker_wav.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "speaker_i2s.pio.h"
#include <limits.h>
#include <string.h>
#if !defined(MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_2_8)
#error Unqualified speaker adapter
#endif
#define BCLK 2
#define DIN 4
static bool owned;
static int generation;
static int sm = -1, dma = -1, offset = -1;
static volatile alarm_id_t alarm;
/* IRQ only touches buffers/PIO/DMA/state; never filesystem or JerryScript. */
static volatile int state; /* 0 idle, 1 sending, 2 drained, 3 draining, -1 underrun */
static volatile unsigned active_buffer;
static volatile size_t ready[2];
static bool last[2];
static uint32_t frames[2][SPEAKER_FRAMES];
static speaker_wav_t wav;
static double gain;
static uint64_t drain_at, playback_deadline;
static bool underrun;
static void silence(void) {
    if (sm >= 0) pio_sm_set_enabled(pio1, sm, false);
    for (unsigned pin=BCLK;pin<=DIN;pin++) {
        gpio_init(pin); gpio_put(pin, false); gpio_set_dir(pin, GPIO_OUT);
    }
}
static void launch(unsigned index) {
    active_buffer = index;
    playback_deadline += ((uint64_t)ready[index] * 1000000u + 15999u) / 16000u;
    dma_channel_set_read_addr(dma, frames[index], false);
    dma_channel_set_trans_count(dma, ready[index], true);
}
static int64_t service(alarm_id_t id, void *unused) {
    (void)unused;
    if (id != alarm) return 0;
    if (state == 1 && !dma_channel_is_busy(dma)) {
        unsigned finished = active_buffer, next = finished ^ 1u;
        ready[finished] = 0;
        /* A ready buffer is not sufficient: a delayed IRQ may already have
         * exhausted the FIFO and clocked zeros. Fail instead of hiding a gap. */
        if (!last[finished] && ready[next] && time_us_64() <= playback_deadline) launch(next);
        else {
            underrun = !last[finished];
            /* DMA completion only means FIFO accepted data. At most 8 FIFO
             * frames plus one OSR frame remain (<563us). Wait >=1ms, while
             * the PIO automatically clocks zeros after the last real frame. */
            drain_at = time_us_64() + 1000;
            state = 3;
        }
    }
    if (state == 3 && time_us_64() >= drain_at) {
        silence(); state = underrun ? -1 : 2; alarm = 0; return 0;
    }
    return 250; // native safety/service, independent of JS completion timer
}
static void stop(void) {
    uint32_t irq = save_and_disable_interrupts();
    alarm_id_t old = alarm; alarm = 0;
    if (old > 0) cancel_alarm(old);
    if (sm >= 0) silence();
    if (dma >= 0) { dma_channel_abort(dma); dma_channel_unclaim(dma); dma = -1; }
    if (sm >= 0) { pio_sm_clear_fifos(pio1,sm); pio_sm_unclaim(pio1,sm); sm = -1; }
    if (offset >= 0) { pio_remove_program(pio1,&speaker_i2s_program,offset); offset = -1; }
    ready[0] = ready[1] = 0; state = 0;
    restore_interrupts(irq);
    speaker_wav_close(&wav);
}
void js_speaker_cleanup(void) { stop(); owned = false; }
static jerry_value_t error(mcujs_operational_error_t code, const char *message) {
    const mcujs_error_details_t details = {.resource="speaker"};
    return mcujs_throw_operational_error(code,message,&details);
}
static jerry_value_t wav_error(speaker_wav_result_t result) {
    if (result == SPEAKER_WAV_UNSUPPORTED) return error(MCUJS_ERROR_NOT_SUPPORTED,"Only PCM WAV 16-bit mono 16000 Hz with 16-byte fmt is supported");
    if (result == SPEAKER_WAV_INVALID) {
        jerry_value_t e = jerry_error_sz(JERRY_ERROR_COMMON,"Malformed or over-limit WAV");
        js_set_string(e,"code","EINVAL"); js_set_string(e,"resource","speaker");
        return jerry_throw_value(e,true);
    }
    if (wav.fs_error == FS_ERROR_BUSY) {
        const mcujs_error_details_t details = {.resource="filesystem",.owner="usb-host"};
        return mcujs_throw_operational_error(MCUJS_ERROR_BUSY,"Filesystem is owned by USB host; eject MCUJS first",&details);
    }
    if (wav.fs_error == FS_ERROR_NOT_FOUND) {
        jerry_value_t e = jerry_error_sz(JERRY_ERROR_COMMON,"WAV file not found");
        js_set_string(e,"code","ENOENT"); return jerry_throw_value(e,true);
    }
    return error(MCUJS_ERROR_IO,"WAV read failed or file was truncated");
}
static bool valid(const jerry_value_t args[], jerry_length_t argc) {
    int token;
    return owned && mcujs_get_integer(args,argc,0,&token)==MCUJS_ARG_OK && token==generation;
}
static speaker_wav_result_t fill(unsigned index) {
    size_t count = 0;
    speaker_wav_result_t result = speaker_wav_read(&wav,frames[index],SPEAKER_FRAMES,gain,&count);
    uint32_t irq = save_and_disable_interrupts();
    last[index] = wav.remaining == 0;
    ready[index] = result == SPEAKER_WAV_OK ? count : 0;
    restore_interrupts(irq);
    return result;
}
static jerry_value_t open_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;(void)args;
    if (argc) return jerry_throw_sz(JERRY_ERROR_TYPE,"speaker.open takes no arguments");
    if (owned) return error(MCUJS_ERROR_BUSY,"Speaker is owned");
    if (generation==INT_MAX) return error(MCUJS_ERROR_RESOURCE_EXHAUSTED,"Speaker tokens exhausted");
    owned = true; return jerry_number(++generation);
}
static jerry_value_t start_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;
    if (!valid(args,argc)) return error(MCUJS_ERROR_NO_DEVICE,"Stale speaker handle");
    if (state || wav.file.is_open) return error(MCUJS_ERROR_BUSY,"Speaker is playing");
    if (argc!=3 || !jerry_value_is_string(args[1]) ||
        mcujs_get_number_range(args,argc,2,0,1,&gain)!=MCUJS_ARG_OK)
        return jerry_throw_sz(JERRY_ERROR_TYPE,"WAV path and volume 0..1 required");
    char path[FS_PATH_MAX];
    jerry_size_t n = jerry_string_size(args[1],JERRY_ENCODING_UTF8);
    if (!n || n>=sizeof(path)) return jerry_throw_sz(JERRY_ERROR_RANGE,"WAV path too long or empty");
    jerry_string_to_buffer(args[1],JERRY_ENCODING_UTF8,(jerry_char_t *)path,n);path[n]=0;
    if (strlen(path)!=n) return jerry_throw_sz(JERRY_ERROR_TYPE,"WAV path contains NUL");
    speaker_wav_result_t result = speaker_wav_open(&wav,path);
    if (result) return wav_error(result);
    if (!wav.remaining) { state=2; return jerry_undefined(); }
    result = fill(0);
    if (!result && wav.remaining) result = fill(1);
    if (result) { stop(); return wav_error(result); }
    sm = pio_claim_unused_sm(pio1,false);
    if (sm<0 || !pio_can_add_program(pio1,&speaker_i2s_program)) goto exhausted;
    offset = pio_add_program(pio1,&speaker_i2s_program);
    dma = dma_claim_unused_channel(false);
    if (dma<0) goto exhausted;
    pio_sm_config config = speaker_i2s_program_get_default_config(offset);
    sm_config_set_out_pins(&config,DIN,1); sm_config_set_sideset_pins(&config,BCLK);
    sm_config_set_out_shift(&config,false,false,32); // MSB first, manual pull
    sm_config_set_fifo_join(&config,PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&config,(float)clock_get_hz(clk_sys)/(16000.0f*192.0f));
    pio_sm_init(pio1,sm,offset,&config);
    pio_sm_set_pins_with_mask(pio1,sm,0,7u<<BCLK);
    pio_sm_set_consecutive_pindirs(pio1,sm,BCLK,3,true);
    for (unsigned pin=BCLK;pin<=DIN;pin++) pio_gpio_init(pio1,pin);
    dma_channel_config dc = dma_channel_get_default_config(dma);
    channel_config_set_transfer_data_size(&dc,DMA_SIZE_32);
    channel_config_set_read_increment(&dc,true); channel_config_set_write_increment(&dc,false);
    channel_config_set_dreq(&dc,pio_get_dreq(pio1,sm,true));
    dma_channel_configure(dma,&dc,&pio1->txf[sm],frames[0],0,false);
    alarm = add_alarm_in_us(250,service,NULL,false);
    if (alarm<=0) { alarm=0; goto exhausted; }
    uint32_t irq = save_and_disable_interrupts();
    playback_deadline=time_us_64();
    state=1; launch(0); pio_sm_set_enabled(pio1,sm,true);
    restore_interrupts(irq);
    return jerry_undefined();
exhausted:
    stop(); return error(MCUJS_ERROR_RESOURCE_EXHAUSTED,"No speaker PIO, DMA or native alarm available");
}
static jerry_value_t poll_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;
    if (!valid(args,argc)) return error(MCUJS_ERROR_NO_DEVICE,"Stale speaker handle");
    if (state==-1) { stop(); return error(MCUJS_ERROR_IO,"Speaker underrun; output silenced"); }
    if (state==1 && wav.remaining) {
        for (unsigned i=0;i<2;i++) if (!ready[i]) {
            speaker_wav_result_t result=fill(i);
            if (result) { stop(); return wav_error(result); }
            break; // at most 1024 bytes / one native refill per JS service turn
        }
    }
    return jerry_number(state==2 ? 2 : 1);
}
static jerry_value_t stop_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;
    if (!valid(args,argc)) return error(MCUJS_ERROR_NO_DEVICE,"Stale speaker handle");
    stop();return jerry_undefined();
}
static jerry_value_t close_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;
    if (!valid(args,argc)) return error(MCUJS_ERROR_NO_DEVICE,"Stale speaker handle");
    js_speaker_cleanup();return jerry_undefined();
}
static jerry_value_t state_handler(const jerry_call_info_t *info,const jerry_value_t args[],jerry_length_t argc) {
    (void)info;(void)args;(void)argc;return jerry_string_sz(owned?"busy":"idle");
}
jerry_value_t js_create_speaker_native_module(void) {
    jerry_value_t module=jerry_object();
    js_set_function(module,"open",open_handler);js_set_function(module,"start",start_handler);
    js_set_function(module,"poll",poll_handler);js_set_function(module,"stop",stop_handler);
    js_set_function(module,"close",close_handler);js_set_function(module,"state",state_handler);
    return module;
}
