#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
typedef unsigned uint;
typedef int32_t alarm_id_t;
typedef int64_t (*alarm_callback_t)(alarm_id_t,void*);
typedef struct { uint32_t txf[4]; } pio_hw_t;
typedef pio_hw_t *PIO;
typedef struct { int unused; } pio_sm_config;
typedef struct { int unused; } dma_channel_config;
typedef struct { int length; } pio_program_t;
extern pio_hw_t test_pio;
#define pio1 (&test_pio)
#define GPIO_OUT true
#define PIO_FIFO_JOIN_TX 1
#define DMA_SIZE_32 2
extern bool test_fail_sm,test_fail_program,test_fail_dma,test_fail_alarm,test_dma_busy,test_enabled;
extern unsigned test_sm_claims,test_program_claims,test_dma_claims,test_launches,test_pin_high;
extern uint64_t test_now;
extern alarm_id_t test_alarm;
extern alarm_callback_t test_callback;
static inline uint32_t save_and_disable_interrupts(void){return 0;}
static inline void restore_interrupts(uint32_t s){(void)s;}
static inline uint64_t time_us_64(void){return test_now;}
static inline alarm_id_t add_alarm_in_us(int64_t us,alarm_callback_t cb,void *p,bool past){(void)p;(void)past;assert(us==250);if(test_fail_alarm)return -1;test_callback=cb;return test_alarm=7;}
static inline bool cancel_alarm(alarm_id_t id){assert(id==test_alarm);test_alarm=0;return true;}
static inline void gpio_init(uint p){assert(p>=2&&p<=4);}
static inline void gpio_put(uint p,bool v){assert(p>=2&&p<=4);if(v)test_pin_high|=1u<<p;else test_pin_high&=~(1u<<p);}
static inline void gpio_set_dir(uint p,bool out){assert(p>=2&&p<=4&&out);}
static inline int pio_claim_unused_sm(PIO p,bool panic){(void)p;assert(!panic);if(test_fail_sm)return -1;assert(!test_sm_claims);test_sm_claims++;return 0;}
static inline bool pio_can_add_program(PIO p,const pio_program_t *pr){(void)p;(void)pr;return !test_fail_program;}
static inline uint pio_add_program(PIO p,const pio_program_t *pr){(void)p;(void)pr;test_program_claims++;return 0;}
static inline void pio_remove_program(PIO p,const pio_program_t *pr,uint off){(void)p;(void)pr;(void)off;assert(test_program_claims);test_program_claims--;}
static inline void pio_sm_unclaim(PIO p,uint sm){(void)p;(void)sm;assert(test_sm_claims);test_sm_claims--;}
static inline void pio_sm_set_enabled(PIO p,uint sm,bool enable){(void)p;(void)sm;test_enabled=enable;}
static inline void pio_sm_clear_fifos(PIO p,uint sm){(void)p;(void)sm;}
static inline void pio_sm_init(PIO p,uint sm,uint off,const pio_sm_config *c){(void)p;(void)sm;(void)off;(void)c;}
static inline void pio_gpio_init(PIO p,uint pin){(void)p;assert(pin>=2&&pin<=4);}
static inline void pio_sm_set_pins_with_mask(PIO p,uint sm,uint32_t val,uint32_t mask){(void)p;(void)sm;assert(!val&&mask==(7u<<2));}
static inline void pio_sm_set_consecutive_pindirs(PIO p,uint sm,uint base,uint count,bool out){(void)p;(void)sm;assert(base==2&&count==3&&out);}
static inline uint pio_get_dreq(PIO p,uint sm,bool tx){(void)p;(void)sm;assert(tx);return 0;}
static inline void sm_config_set_out_pins(pio_sm_config *c,uint pin,uint count){(void)c;assert(pin==4&&count==1);}
static inline void sm_config_set_sideset_pins(pio_sm_config *c,uint pin){(void)c;assert(pin==2);}
static inline void sm_config_set_out_shift(pio_sm_config *c,bool right,bool automatic,uint bits){(void)c;assert(!right&&!automatic&&bits==32);}
static inline void sm_config_set_fifo_join(pio_sm_config *c,int join){(void)c;assert(join==PIO_FIFO_JOIN_TX);}
static inline void sm_config_set_clkdiv(pio_sm_config *c,float div){(void)c;assert(div==48.828125f);}
#define clk_sys 0
static inline uint32_t clock_get_hz(int clock){(void)clock;return 150000000;}
static inline int dma_claim_unused_channel(bool panic){assert(!panic);if(test_fail_dma)return -1;assert(!test_dma_claims);test_dma_claims++;return 0;}
static inline void dma_channel_unclaim(uint c){(void)c;assert(test_dma_claims);test_dma_claims--;}
static inline bool dma_channel_is_busy(uint c){(void)c;return test_dma_busy;}
static inline void dma_channel_abort(uint c){(void)c;test_dma_busy=false;}
static inline void dma_channel_set_read_addr(uint c,const void *addr,bool trigger){(void)c;assert(addr&&!trigger);}
static inline void dma_channel_set_trans_count(uint c,uint32_t count,bool trigger){(void)c;assert(count>0&&count<=512&&trigger);test_dma_busy=true;test_launches++;}
static inline dma_channel_config dma_channel_get_default_config(uint c){(void)c;return (dma_channel_config){0};}
static inline void channel_config_set_transfer_data_size(dma_channel_config *c,int size){(void)c;assert(size==DMA_SIZE_32);}
static inline void channel_config_set_read_increment(dma_channel_config *c,bool v){(void)c;assert(v);}
static inline void channel_config_set_write_increment(dma_channel_config *c,bool v){(void)c;assert(!v);}
static inline void channel_config_set_dreq(dma_channel_config *c,uint dreq){(void)c;(void)dreq;}
static inline void dma_channel_configure(uint c,const dma_channel_config *config,volatile void *dst,const void *src,uint count,bool trigger){(void)c;(void)config;assert(dst&&src&&!count&&!trigger);}
static const pio_program_t speaker_i2s_program={11};
static inline pio_sm_config speaker_i2s_program_get_default_config(uint offset){(void)offset;return (pio_sm_config){0};}
