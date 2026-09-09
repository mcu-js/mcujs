const assert = require('node:assert/strict');
const {readFileSync,writeFileSync,mkdtempSync,rmSync} = require('node:fs');
const {join} = require('node:path');
const {tmpdir} = require('node:os');
const {execFileSync} = require('node:child_process');
const test = require('node:test');

test('RP USB reset requests an intentional normal boot, not watchdog-failure recovery', () => {
  const source = readFileSync(join(__dirname,'../platform/rp2/usb/usb_cdc.c'),'utf8');
  const body = source.match(/^void usb_cdc_reset_usb\(uint32_t delay_ms\) \{[\s\S]*?^\}/m);
  assert.ok(body, 'production reset function must be present');
  const dir = mkdtempSync(join(tmpdir(),'mcujs-usb-reset-'));
  try {
    writeFileSync(join(dir,'test.c'), `
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdlib.h>
static jmp_buf rebooted;
static int stage, intentional;
void tud_disconnect(void) { assert(stage==0);stage=1; }
void sleep_ms(uint32_t ms) { assert(stage==1 && ms==250);stage=2; }
void watchdog_enable(uint32_t ms, bool debug) { (void)ms;(void)debug;longjmp(rebooted,1); }
void watchdog_reboot(uint32_t pc,uint32_t sp,uint32_t ms) { assert(stage==2 && pc==0 && sp==0 && ms==1);intentional=1;longjmp(rebooted,1); }
void tight_loop_contents(void) { abort(); }
${body[0]}
int main(void) { if(!setjmp(rebooted))usb_cdc_reset_usb(250);assert(intentional);return 0; }
`);
    execFileSync('cc',['-std=c11','-Wall','-Wextra','-Werror',join(dir,'test.c'),'-o',join(dir,'test')]);
    execFileSync(join(dir,'test'));
  } finally { rmSync(dir,{recursive:true,force:true}); }
});
