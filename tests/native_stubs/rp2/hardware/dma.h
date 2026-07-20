#ifndef MCUJS_TEST_RP2_DMA_H
#define MCUJS_TEST_RP2_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t unused;
} dma_channel_config;

#define DMA_SIZE_8 0
#define NUM_DMA_CHANNELS 12

int dma_claim_unused_channel(bool required);
dma_channel_config dma_channel_get_default_config(int channel);
void channel_config_set_transfer_data_size(dma_channel_config *config, int size);
void channel_config_set_dreq(dma_channel_config *config, unsigned dreq);
void channel_config_set_read_increment(dma_channel_config *config, bool increment);
void channel_config_set_write_increment(dma_channel_config *config, bool increment);
void dma_channel_configure(int channel, const dma_channel_config *config,
                           volatile void *write_address, const void *read_address,
                           size_t transfer_count, bool trigger);
void dma_channel_wait_for_finish_blocking(int channel);

#endif
