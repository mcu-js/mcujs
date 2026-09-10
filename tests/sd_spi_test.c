/* Native PL022/FatFs seam: production sd_spi.c is a separate translation unit. */
#include "sd_spi.h"
#include "board_config.h"
#include "hardware/spi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static spi_inst_t instance = {1};
spi_inst_t *spi1 = &instance;
static spi_hw_t hw;
static uint64_t now;
static unsigned transfers, polls, baud, packet_size, qhead, qtail, ncommands;
static unsigned commands[22000];
static bool receiving_write;
static unsigned write_pos, writes;
static uint8_t write_bytes[514];
static uint32_t write_sector;
static uint8_t packet[6], queue[2048];
static bool selected, absent, freeze_time, bad_echo, sdsc, crc_rejected;
static bool acmd_stuck, bad_csd_crc;
static bool bad_read_crc, missing_token, busy_forever, write_busy, missing_accept;
static bool removed_status;
static unsigned remove_at;
static uint8_t read_token, acceptance, status_r2;
static unsigned rejected_command;
static uint16_t csd_crc;
static unsigned cases;
static uint16_t reference_crc16(const uint8_t *data, unsigned n) {
    static const uint16_t table[16] = {0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
        0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef};
    uint16_t crc=0;
    for (unsigned i=0; i<n; ++i) {
        crc=(uint16_t)((crc<<4)^table[((crc>>12)^(data[i]>>4))&15]);
        crc=(uint16_t)((crc<<4)^table[((crc>>12)^data[i])&15]);
    }
    return crc;
}
static int stall;
static unsigned init_attempts;
/* Constructed SDHC CSD v2: C_SIZE=8191 => 8,388,608 sectors (4 GiB).
 * CRC7 by polynomial long division; CRC16 independently checked with
 * Python binascii.crc_hqx(bytes.fromhex(...), 0) == 0x2c75.
 */
static const uint8_t known_csd[16] = {
    0x40,0x0e,0x00,0x32,0x5b,0x59,0x00,0x00,
    0x1f,0xff,0x7f,0x80,0x0a,0x40,0x00,0xc3
};
static uint8_t csd[16];

static uint8_t reference_crc7(const uint8_t *bytes, unsigned length) {
    unsigned remainder = 0;
    for (unsigned i = 0; i < length; ++i) {
        remainder = (remainder << 8) | bytes[i];
        for (int bit = 14; bit >= 7; --bit)
            if (remainder & (1u << bit)) remainder ^= 0x89u << (bit - 7);
    }
    remainder <<= 7;
    for (int bit = 13; bit >= 7; --bit)
        if (remainder & (1u << bit)) remainder ^= 0x89u << (bit - 7);
    return (uint8_t)((remainder << 1) | 1);
}
static void push(uint8_t value) { assert(qtail < sizeof(queue)); queue[qtail++] = value; }
static void reply_command(void) {
    unsigned cmd = packet[0] & 63;
    uint32_t arg = ((uint32_t)packet[1] << 24) | ((uint32_t)packet[2] << 16) |
                   ((uint32_t)packet[3] << 8) | packet[4];
    assert(packet[5] == reference_crc7(packet, 5));
    assert(ncommands < sizeof(commands)/sizeof(commands[0]));
    commands[ncommands++] = cmd;
    if (cmd == rejected_command) { push(4); return; }
    switch (cmd) {
    case 0: assert(arg == 0 && packet[5] == 0x95); push(1); break;
    case 8:
        assert(arg == 0x1aa && packet[5] == 0x87);
        push(1); push(0); push(0); push(1); push(bad_echo ? 0xab : 0xaa); break;
    case 59: assert(arg == 1); push(crc_rejected ? 5 : 1); break;
    case 55: assert(arg == 0); push(1); break;
    case 41:
        assert(arg == 0x40000000); ++init_attempts;
        push(acmd_stuck || init_attempts < 3 ? 1 : 0); break;
    case 58:
        assert(arg == 0); push(0); push(sdsc ? 0x80 : 0xc0);
        push(0xff); push(0x80); push(0); break;
    case 9:
        assert(arg == 0); push(0); push(0xff); push(0xfe);
        for (unsigned i=0; i<sizeof(csd); ++i) push(csd[i]);
        push((uint8_t)(csd_crc >> 8)); push((uint8_t)(csd_crc ^ (bad_csd_crc ? 1 : 0))); break;
    case 17:
        assert(arg == 7 || arg == 8 || arg == 8388607);
        push(0); push(0xff);
        if (missing_token) break;
        push(read_token);
        if (read_token != 0xfe) break;
        for (unsigned i=0; i<512; ++i) push(arg == 8 ? 0 : 0xff);
        push(arg == 8 ? 0 : 0x7f); push((arg == 8 ? 0 : 0xa1) ^ (bad_read_crc ? 1 : 0)); break;
    case 24:
        assert(arg == 7 || arg == 8);
        write_sector=arg; write_pos=0; receiving_write=true; push(0); break;
    case 13:
        assert(arg == 0);
        if (removed_status) { absent=true; break; }
        push(0); push(status_r2); break;
    default: assert(!"unexpected SD command");
    }
}
static uint8_t exchange(uint8_t tx) {
    ++transfers;
    assert(transfers < 1000000); /* Independent hard stop for unbounded driver loops. */
    if (!freeze_time) now += baud ? 8000000 / baud : 20;
    if (remove_at && transfers >= remove_at) absent=true;
    if (!selected || absent) return 0xff;
    if (busy_forever) return 0;
    if (qhead < qtail) {
        uint8_t rx = queue[qhead++];
        if (qhead == qtail) qhead = qtail = 0;
        return rx;
    }
    if (write_busy && writes) return 0;
    if (receiving_write) {
        if (!write_pos) {
            if (tx == 0xff) return 0xff;
            assert(tx == 0xfe); write_pos=1; return 0xff;
        }
        write_bytes[write_pos++ - 1]=tx;
        if (write_pos == 515) {
            for (unsigned i=0; i<512; ++i) assert(write_bytes[i] == (write_sector==8 ? 0 : 0xff));
            assert(write_bytes[512] == (write_sector==8 ? 0 : 0x7f));
            assert(write_bytes[513] == (write_sector==8 ? 0 : 0xa1));
            ++writes; receiving_write=false;
            if (!missing_accept) {
                push(acceptance); push(0); push(0);
                if (!write_busy) push(0xff);
            }
        }
        return 0xff;
    }
    if (packet_size || (tx & 0xc0) == 0x40) {
        packet[packet_size++] = tx;
        if (packet_size == 6) { packet_size = 0; reply_command(); }
    }
    return 0xff;
}
uint64_t time_us_64(void) { if (!freeze_time) ++now; return now; }
void sleep_ms(uint ms) { if (!freeze_time) now += ms * 1000; }
void gpio_init(uint pin) { assert(pin == MCUJS_SD_CS_PIN); }
void gpio_set_dir(uint pin, bool output) { assert(pin == MCUJS_SD_CS_PIN && output); }
void gpio_put(uint pin, bool value) {
    assert(pin == MCUJS_SD_CS_PIN);
    selected = !value;
    if (value) { packet_size = qhead = qtail = 0; }
}
void gpio_set_function(uint pin, uint function) {
    assert(function == GPIO_FUNC_SPI);
    assert(pin == MCUJS_SD_SCK_PIN || pin == MCUJS_SD_MOSI_PIN || pin == MCUJS_SD_MISO_PIN);
}
void gpio_pull_up(uint pin) { assert(pin == MCUJS_SD_MISO_PIN); }
uint spi_init(spi_inst_t *spi, uint value) { assert(spi == spi1); baud=value; return value; }
uint spi_set_baudrate(spi_inst_t *spi, uint value) { assert(spi == spi1); baud=value; return value; }
void spi_deinit(spi_inst_t *spi) { assert(spi == spi1); }
void spi_set_format(spi_inst_t *spi, uint bits, int cpol, int cpha, int order) {
    assert(spi==spi1 && bits==8 && cpol==0 && cpha==0 && order==SPI_MSB_FIRST);
}
spi_hw_t *spi_get_hw(spi_inst_t *spi) { assert(spi==spi1); return &hw; }
static void poll(void) { ++polls; assert(polls < 3000000); }
bool spi_is_writable(spi_inst_t *spi) { assert(spi==spi1); poll(); return stall != 1; }
bool spi_is_readable(spi_inst_t *spi) {
    assert(spi==spi1); poll();
    if (stall == 2) return false;
    hw.dr = exchange((uint8_t)hw.dr);
    return true;
}
bool spi_is_busy(spi_inst_t *spi) { assert(spi==spi1); poll(); return stall == 3; }
static void reset_mock(void) {
    now=0; transfers=polls=baud=packet_size=qhead=qtail=ncommands=init_attempts=0;
    selected=absent=freeze_time=bad_echo=sdsc=crc_rejected=acmd_stuck=bad_csd_crc=false;
    stall=0; receiving_write=false; write_pos=writes=0;
    memcpy(csd, known_csd, sizeof(csd));
    bad_read_crc=missing_token=busy_forever=write_busy=missing_accept=removed_status=false;
    remove_at=0; read_token=0xfe; acceptance=5; status_r2=0;
    rejected_command=255; csd_crc=0x2c75;
}
static void csd_changed(void) {
    csd[15]=reference_crc7(csd,15);
    csd_crc=reference_crc16(csd,16);
}
static void init_ok(void) { reset_mock(); assert(mcujs_sd_initialize() == 0); }
static void rejected(void) {
    assert(mcujs_sd_initialize() & STA_NOINIT);
    assert(!selected && writes == 0 && transfers <= 300001);
    BYTE data[512];
    assert(mcujs_sd_write(data, 7, 1) == RES_NOTRDY);
    ++cases;
}
static void failed_io(DRESULT result) {
    assert(result == RES_ERROR);
    assert(mcujs_sd_status() & STA_NOINIT);
    assert(!selected && transfers < 301000);
    ++cases;
}
int main(void) {
    assert(mcujs_sd_status() & STA_NOINIT);
    reset_mock(); absent=true;
    assert(mcujs_sd_initialize() & STA_NOINIT);
    reset_mock();
    assert(mcujs_sd_initialize() == 0);
    assert(mcujs_sd_status() == 0);
    assert(baud == 5000000 && !selected);
    const unsigned expected[] = {0,8,59,55,41,55,41,55,41,58,9};
    assert(ncommands == sizeof(expected)/sizeof(expected[0]));
    assert(memcmp(commands, expected, sizeof(expected)) == 0);
    BYTE data[1024];
    LBA_t sectors=0;
    WORD size=0;
    assert(mcujs_sd_ioctl(GET_SECTOR_COUNT, &sectors) == RES_OK && sectors == 8388608);
    assert(mcujs_sd_ioctl(GET_SECTOR_SIZE, &size) == RES_OK && size == 512);
    assert(mcujs_sd_read(data, 7, 2) == RES_OK);
    for (unsigned i=0; i<512; ++i) assert(data[i] == 0xff && data[i+512] == 0);
    assert(!selected);
    assert(mcujs_sd_write(data, 7, 2) == RES_OK);
    assert(writes == 2 && !selected);
    assert(commands[ncommands-1] == 13);
    assert(mcujs_sd_ioctl(CTRL_SYNC, NULL) == RES_OK);
    ++cases;
    /* Invalid calls must not send a byte or invalidate a healthy card. */
    unsigned before=transfers;
    assert(mcujs_sd_read(NULL, 7, 1) == RES_PARERR);
    assert(mcujs_sd_read(data, 7, 0) == RES_PARERR);
    assert(mcujs_sd_read(data, 8388608, 1) == RES_PARERR);
    assert(mcujs_sd_read(data, 8388607, 2) == RES_PARERR);
    assert(mcujs_sd_read(data, (LBA_t)-1, 2) == RES_PARERR);
    assert(mcujs_sd_write(NULL, 7, 1) == RES_PARERR);
    assert(mcujs_sd_write(data, 7, 0) == RES_PARERR);
    assert(mcujs_sd_write(data, 8388607, 2) == RES_PARERR);
    assert(mcujs_sd_write(data, (LBA_t)-1, 1) == RES_PARERR);
    assert(mcujs_sd_ioctl(GET_SECTOR_COUNT, NULL) == RES_PARERR);
    assert(mcujs_sd_ioctl(CTRL_TRIM, data) == RES_PARERR);
    assert(mcujs_sd_ioctl(255, data) == RES_PARERR);
    assert(transfers == before && mcujs_sd_status() == 0);
    DWORD block=0;
    assert(mcujs_sd_ioctl(GET_BLOCK_SIZE, &block) == RES_OK && block == 1);
    assert(mcujs_sd_read(data, 8388607, 1) == RES_OK);
    ++cases;
    reset_mock(); absent=true; rejected();
    reset_mock(); bad_echo=true; rejected();
    reset_mock(); sdsc=true; rejected();
    reset_mock(); crc_rejected=true; rejected();
    reset_mock(); bad_csd_crc=true; rejected();
    reset_mock(); csd[0]=0; csd_changed(); rejected();
    reset_mock(); csd[5]=(csd[5]&0xf0)|10; csd_changed(); rejected();
    reset_mock(); csd[13]=0; csd_changed(); rejected();
    reset_mock(); csd[15]^=2; csd_crc=reference_crc16(csd,16); rejected();
    const unsigned init_commands[]={0,8,59,55,41,58,9};
    for (unsigned i=0; i<sizeof(init_commands)/sizeof(init_commands[0]); ++i) {
        reset_mock(); rejected_command=init_commands[i]; rejected();
    }
    for (unsigned bit=0x10; bit<=0x20; bit<<=1) {
        reset_mock(); csd[14]|=bit; csd_changed();
        assert(mcujs_sd_initialize() == STA_PROTECT);
        before=transfers;
        assert(mcujs_sd_write(data, 7, 1) == RES_WRPRT);
        assert(writes == 0 && transfers == before);
        assert(mcujs_sd_read(data, 7, 1) == RES_OK);
        ++cases;
    }
    reset_mock(); csd[7]=0x3f; csd[8]=csd[9]=0xff; csd_changed();
#ifdef SD_TEST_LBA64
    assert(mcujs_sd_initialize() == 0);
    assert(mcujs_sd_ioctl(GET_SECTOR_COUNT, &sectors) == RES_OK);
    assert(sectors == UINT64_C(4294967296)); ++cases;
#else
    rejected();
#endif
    for (unsigned frozen=0; frozen<2; ++frozen) {
        reset_mock(); freeze_time=frozen; acmd_stuck=true; rejected();
        assert(frozen || now < 2010000);
        for (int fault=1; fault<=3; ++fault) {
            reset_mock(); stall=fault; freeze_time=frozen; rejected();
            assert(polls < 10000);
            init_ok(); stall=fault; freeze_time=frozen;
            failed_io(mcujs_sd_read(data, 7, 1));
        }
        init_ok(); missing_token=true; freeze_time=frozen;
        uint64_t started=now;
        failed_io(mcujs_sd_read(data, 7, 1));
        assert(frozen || now-started < 210000);
        init_ok(); busy_forever=true; freeze_time=frozen;
        started=now; failed_io(mcujs_sd_read(data, 7, 1));
        assert(frozen || now-started < 510000);
        init_ok(); write_busy=true; freeze_time=frozen;
        started=now; failed_io(mcujs_sd_write(data, 7, 1));
        assert(frozen || now-started < 520000);
    }
    init_ok(); bad_read_crc=true; failed_io(mcujs_sd_read(data,7,1));
    init_ok(); read_token=0x0b; failed_io(mcujs_sd_read(data,7,1));
    init_ok(); rejected_command=17; failed_io(mcujs_sd_read(data,7,1));
    init_ok(); absent=true; failed_io(mcujs_sd_read(data,7,1));
    init_ok(); remove_at=transfers+100; failed_io(mcujs_sd_read(data,8,1));
    memset(data, 0xff, sizeof(data));
    init_ok(); acceptance=0x0b; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); acceptance=0x0d; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); missing_accept=true; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); rejected_command=24; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); status_r2=4; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); removed_status=true; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); remove_at=transfers+100; failed_io(mcujs_sd_write(data,7,1));
    init_ok(); status_r2=0x20;
    assert(mcujs_sd_write(data,7,1) == RES_WRPRT);
    assert(mcujs_sd_status() & STA_NOINIT); ++cases;
    init_ok(); absent=true; failed_io(mcujs_sd_ioctl(CTRL_SYNC,NULL));
    assert(mcujs_sd_ioctl(CTRL_SYNC,NULL) == RES_NOTRDY);
    assert(mcujs_sd_read(data,7,1) == RES_NOTRDY);
    init_ok(); assert(mcujs_sd_read(data,7,1) == RES_OK); ++cases;
    printf("sd_spi: %u cases passed (LBA%u): init/CRC/read/write/geometry/protection/removal/bounded faults\n",
           cases, (unsigned)(sizeof(LBA_t)*8));
    return 0;
}
