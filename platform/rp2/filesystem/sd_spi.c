/*
 * Exclusive SPI1 SDHC/SDXC block device, SD v2 only, mode 0, 512-byte sectors.
 * No DMA/IRQ/core work, formatting, erase, TRIM, or automatic retry of writes.
 * The caller serializes access and reserves the bus/pins. No card-detect or
 * socket write-protect signal exists: status is cached; I/O detects removal.
 * CRC is mandatory (CMD59); command CRC7 and read/write data CRC16 are checked.
 */
#include "board_config.h"
#if MCUJS_HAS_SD
#include "sd_spi.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "usb_cdc.h"

#if MCUJS_SD_SPI_BUS != 1
#error "The SD slice requires exclusive SPI1"
#endif

#define SD_SECTOR_SIZE 512u
#define SD_INIT_US 2000000u
#define SD_IO_US 1000000u
#define SD_BYTE_US 2000u
#define SD_BYTE_POLLS 4096u
#define SD_TRANSFER_LIMIT 300000u
#define SD_READY_US 500000u
#define SD_TOKEN_US 200000u

static DSTATUS status = STA_NOINIT;
static LBA_t sector_count;
/* Every wire operation has BOTH a wall-clock deadline and a byte budget.
 * Every MMIO polling stage also has a deadline and a poll budget. In
 * particular, do not replace this with SDK spi_*_blocking (no timeout).
 */
static uint64_t deadline;
static unsigned remaining;

static void begin(unsigned duration) {
    deadline = time_us_64() + duration;
    remaining = SD_TRANSFER_LIMIT;
}

static bool wait_hw(unsigned stage, uint64_t until) {
    for (unsigned i = 0; i < SD_BYTE_POLLS; ++i) {
        if (time_us_64() >= until || time_us_64() >= deadline) return false;
        if ((stage == 0 && spi_is_writable(spi1)) ||
            (stage == 1 && spi_is_readable(spi1)) ||
            (stage == 2 && !spi_is_busy(spi1))) return true;
    }
    return false;
}

static bool transfer(uint8_t tx, uint8_t *rx) {
    if (!remaining || time_us_64() >= deadline) return false;
    --remaining;
    uint64_t until = time_us_64() + SD_BYTE_US;
    if (!wait_hw(0, until)) return false;
    spi_get_hw(spi1)->dr = tx;
    if (!wait_hw(1, until)) return false;
    uint8_t value = (uint8_t)spi_get_hw(spi1)->dr;
    if (!wait_hw(2, until)) return false;
    if (rx) *rx = value;
    return true;
}

static bool receive(uint8_t *rx) { return transfer(0xff, rx); }

static bool release(void) {
    gpio_put(MCUJS_SD_CS_PIN, 1);
    return receive(NULL); /* Release DO, also on the last transaction. */
}

static DRESULT fail_at(unsigned line) {
    char message[64];
    int length=snprintf(message,sizeof(message),"[sd] I/O fault at driver line %u\r\n",line);
    for (int i=0; i<length && i<(int)sizeof(message)-1; i++) usb_cdc_putchar(message[i]);
    status = STA_NOINIT;
    sector_count = 0;
    (void)release();
    spi_deinit(spi1); /* Clear stalled/in-flight FIFO state before a retry. */
    return RES_ERROR;
}

#define fail() fail_at(__LINE__)

static bool wait_ready(void) {
    uint64_t until = time_us_64() + SD_READY_US;
    for (unsigned i = 0; i < SD_TRANSFER_LIMIT; ++i) {
        uint8_t value;
        if (time_us_64() >= until || !receive(&value)) return false;
        if (value == 0xff) return true;
    }
    return false;
}

static uint8_t crc7(const uint8_t *data, unsigned length) {
    uint8_t crc = 0;
    for (unsigned i = 0; i < length; ++i) {
        uint8_t value = data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc <<= 1;
            if ((value ^ crc) & 0x80) crc ^= 0x09;
            value <<= 1;
        }
    }
    return (uint8_t)((crc << 1) | 1);
}

static uint16_t crc16(const uint8_t *data, unsigned length) {
    uint16_t crc = 0;
    for (unsigned i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
}

/* Leaves CS asserted for a following response or data block. */
static bool command(uint8_t cmd, uint32_t arg, uint8_t *r1) {
    if (!release()) return false;
    gpio_put(MCUJS_SD_CS_PIN, 0);
    if (!wait_ready()) return false;
    uint8_t packet[6] = { (uint8_t)(0x40 | cmd), (uint8_t)(arg >> 24),
        (uint8_t)(arg >> 16), (uint8_t)(arg >> 8), (uint8_t)arg, 0 };
    packet[5] = crc7(packet, 5);
    for (unsigned i = 0; i < sizeof(packet); ++i)
        if (!transfer(packet[i], NULL)) return false;
    /* Ncr is at most eight bytes; a small finite margin is harmless. */
    for (unsigned i = 0; i < 16; ++i) {
        if (!receive(r1)) return false;
        if (!(*r1 & 0x80)) return true;
    }
    return false;
}

static bool read_data(uint8_t *buffer, unsigned length) {
    uint64_t until = time_us_64() + SD_TOKEN_US;
    uint8_t token = 0xff;
    for (unsigned i = 0; i < SD_TRANSFER_LIMIT; ++i) {
        if (time_us_64() >= until || !receive(&token)) return false;
        if (token != 0xff) break;
    }
    if (token != 0xfe) return false; /* Includes card data-error tokens. */
    for (unsigned i = 0; i < length; ++i)
        if (!receive(&buffer[i])) return false;
    uint8_t high, low;
    if (!receive(&high) || !receive(&low)) return false;
    return crc16(buffer, length) == (uint16_t)((high << 8) | low);
}

static DRESULT validate(const void *buffer, LBA_t sector, UINT count) {
    if (!buffer || !count || (uint64_t)count * SD_SECTOR_SIZE > SIZE_MAX) return RES_PARERR;
    if (status & STA_NOINIT) return RES_NOTRDY;
    if (sector >= sector_count || (uint64_t)count > (uint64_t)sector_count - sector)
        return RES_PARERR;
    return RES_OK;
}

DRESULT mcujs_sd_read(BYTE *buffer, LBA_t sector, UINT count) {
    DRESULT result = validate(buffer, sector, count);
    if (result != RES_OK) return result;
    for (UINT i = 0; i < count; ++i) {
        begin(SD_IO_US);
        uint8_t r1;
        if (!command(17, (uint32_t)(sector + i), &r1) || r1 != 0 ||
            !read_data(buffer, SD_SECTOR_SIZE) || !release()) return fail();
        buffer += SD_SECTOR_SIZE;
    }
    return RES_OK;
}

/* R2 catches programming errors, write protection, and removal (floating DO
 * can look ready to a busy poll, but cannot produce a valid CMD13 response). */
static DRESULT card_status(void) {
    uint8_t r1, r2;
    if (!command(13, 0, &r1) || !receive(&r2) || !release()) return fail();
    if (r1 || r2) {
        (void)fail();
        return (r2 & 0x20) ? RES_WRPRT : RES_ERROR;
    }
    return RES_OK;
}

DRESULT mcujs_sd_ioctl(BYTE cmd, void *buffer) {
    if (cmd != CTRL_SYNC && cmd != GET_SECTOR_COUNT && cmd != GET_SECTOR_SIZE &&
        cmd != GET_BLOCK_SIZE) return RES_PARERR; /* Deliberately no CTRL_TRIM. */
    if (cmd != CTRL_SYNC && !buffer) return RES_PARERR;
    if (status & STA_NOINIT) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC:
        begin(SD_IO_US);
        return card_status();
    case GET_SECTOR_COUNT: *(LBA_t *)buffer = sector_count; break;
    case GET_SECTOR_SIZE: *(WORD *)buffer = SD_SECTOR_SIZE; break;
    case GET_BLOCK_SIZE: *(DWORD *)buffer = 1; break; /* Unknown erase geometry. */
    default: return RES_PARERR;
    }
    return RES_OK;
}
DRESULT mcujs_sd_write(const BYTE *buffer, LBA_t sector, UINT count) {
    DRESULT result = validate(buffer, sector, count);
    if (result != RES_OK) return result;
    if (status & STA_PROTECT) return RES_WRPRT;
    for (UINT i = 0; i < count; ++i) {
        begin(SD_IO_US);
        uint8_t r1;
        if (!command(24, (uint32_t)(sector + i), &r1) || r1 != 0 ||
            !receive(NULL) || !transfer(0xfe, NULL)) return fail();
        uint16_t crc = crc16(buffer, SD_SECTOR_SIZE);
        for (unsigned j = 0; j < SD_SECTOR_SIZE; ++j)
            if (!transfer(buffer[j], NULL)) return fail();
        if (!transfer((uint8_t)(crc >> 8), NULL) || !transfer((uint8_t)crc, NULL)) return fail();
        uint8_t response = 0xff;
        for (unsigned j = 0; j < 16; ++j) {
            if (!receive(&response)) return fail();
            if (response != 0xff) break;
        }
        if ((response & 0x1f) != 5 || !wait_ready()) return fail();
        result = card_status();
        if (result != RES_OK) return result;
        buffer += SD_SECTOR_SIZE;
    }
    return RES_OK;
}

DSTATUS mcujs_sd_status(void) { return status; }

DSTATUS mcujs_sd_initialize(void) {
    status = STA_NOINIT;
    sector_count = 0;
    gpio_init(MCUJS_SD_CS_PIN);
    gpio_put(MCUJS_SD_CS_PIN, 1); /* Set latch before enabling output. */
    gpio_set_dir(MCUJS_SD_CS_PIN, GPIO_OUT);
    spi_init(spi1, 400000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(MCUJS_SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MCUJS_SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MCUJS_SD_MISO_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(MCUJS_SD_MISO_PIN);
    sleep_ms(2); /* SD power-up requires >=1 ms before the initial clocks. */
    begin(SD_INIT_US);
    for (unsigned i = 0; i < 10; ++i) if (!receive(NULL)) goto error;
    uint8_t r1, r7[4], ocr[4], csd[16];
    if (!command(0, 0, &r1) || r1 != 1) goto error;
    if (!command(8, 0x1aa, &r1) || r1 != 1) goto error;
    for (unsigned i = 0; i < sizeof(r7); ++i) if (!receive(&r7[i])) goto error;
    if (r7[0] || r7[1] || r7[2] != 1 || r7[3] != 0xaa) goto error;
    if (!command(59, 1, &r1) || r1 != 1) goto error;
    for (unsigned i = 0; i < 10000; ++i) {
        if (!command(55, 0, &r1) || r1 != 1) goto error;
        if (!command(41, 0x40000000, &r1)) goto error;
        if (r1 == 0) break;
        if (r1 != 1 || i == 9999) goto error;
    }
    if (!command(58, 0, &r1) || r1 != 0) goto error;
    for (unsigned i = 0; i < sizeof(ocr); ++i) if (!receive(&ocr[i])) goto error;
    if ((ocr[0] & 0xc0) != 0xc0 || !(ocr[1] & 0x30)) goto error;
    if (!command(9, 0, &r1) || r1 != 0 || !read_data(csd, sizeof(csd))) goto error;
    if ((csd[0] >> 6) != 1 || (csd[5] & 15) != 9 ||
        (((csd[12] & 3) << 2) | (csd[13] >> 6)) != 9 ||
        csd[15] != crc7(csd, 15)) goto error;
    uint32_t size = ((uint32_t)(csd[7] & 63) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
    uint64_t sectors = ((uint64_t)size + 1) * 1024;
    /* GET_SECTOR_COUNT must be representable, not silently wrap at 2 TiB. */
    if (sectors > (LBA_t)-1) goto error;
    sector_count = (LBA_t)sectors;
    if (!release()) goto error;
    spi_set_baudrate(spi1, 5000000);
    status = (csd[14] & 0x30) ? STA_PROTECT : 0;
    return status;
error:
    (void)fail();
    return status;
}
#endif
