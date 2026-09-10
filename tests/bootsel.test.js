'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync, mkdtempSync, writeFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');
const { spawnSync } = require('node:child_process');
const source = readFileSync(join(__dirname, '../platform/rp2/boot.c'), 'utf8');
// Exercise the production SRAM sampler with independent SDK-register fixtures.
const sampler = source.match(/^bool __no_inline_not_in_flash_func\(boot_button_pressed\)[\s\S]*?^}/m)[0];
for (const rp2350 of [0, 1]) {
  test(`BOOTSEL preserves controls/interrupts and samples the right RP${rp2350 ? '2350' : '2040'} input`, () => {
    const dir = mkdtempSync(join(tmpdir(), 'mcujs-bootsel-'));
    try {
      const fixture = `
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
typedef unsigned uint;
#define __no_inline_not_in_flash_func(n) n
#define PICO_RP2350 ${rp2350}
// Independent literal from pinned RP2350 SDK hardware/regs/sio.h.
#define SIO_GPIO_HI_IN_QSPI_CSN_BITS 0x08000000u
#define GPIO_OVERRIDE_LOW 2u
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB 12u
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS (3u << 12u)
static struct { struct { uint32_t ctrl; } io[6]; } qspi;
static struct { uint32_t gpio_hi_in; } sio;
#define ioqspi_hw (&qspi)
#define sio_hw (&sio)
static bool disabled;
static uint32_t save_and_disable_interrupts(void) { assert(!disabled); disabled=true; return 42; }
static void restore_interrupts(uint32_t old) { assert(disabled && old==42); disabled=false; }
static void hw_write_masked(uint32_t *reg,uint32_t value,uint32_t mask) { assert(disabled); *reg=(*reg & ~mask)|(value & mask); }
${sampler}
int main(void) {
  const uint32_t cs = ${rp2350 ? '0x08000000u' : '2u'};
  qspi.io[1].ctrl=0x12345678u;
  sio.gpio_hi_in=cs; assert(!boot_button_pressed());
  assert(qspi.io[1].ctrl==0x12345678u && !disabled);
  sio.gpio_hi_in=~cs; assert(boot_button_pressed());
  assert(qspi.io[1].ctrl==0x12345678u && !disabled);
  return 0;
}
`;
      writeFileSync(join(dir, 'test.c'), fixture);
      const compiled = spawnSync('cc', ['-std=c11', '-Wall', '-Wextra', '-Werror', join(dir, 'test.c'), '-o', join(dir, 'test')], { encoding: 'utf8' });
      assert.equal(compiled.status, 0, compiled.stderr);
      const executed = spawnSync(join(dir, 'test'), [], { encoding: 'utf8' });
      assert.equal(executed.status, 0, executed.stderr);
    } finally { rmSync(dir, { recursive: true, force: true }); }
  });
}
