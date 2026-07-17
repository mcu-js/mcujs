/*
 * mcujs - Flash Operations Implementation
 */

#include "flash_ops.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include <string.h>

void flash_ops_read(uint32_t addr, void *buffer, size_t size) {
    memcpy(buffer, (void *)addr, size);
}

void flash_ops_write(uint32_t addr, const void *buffer, size_t size) {
    /* Convert to flash offset (relative to XIP_BASE) */
    uint32_t flash_offset = addr - XIP_BASE;
    
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(flash_offset, buffer, size);
    restore_interrupts(ints);
}

void flash_ops_erase(uint32_t addr, size_t size) {
    /* Convert to flash offset (relative to XIP_BASE) */
    uint32_t flash_offset = addr - XIP_BASE;
    
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, size);
    restore_interrupts(ints);
}
