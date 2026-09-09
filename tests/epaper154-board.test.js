const {test}=require('node:test');
const assert=require('node:assert/strict');
const {boardDescriptors}=require('../runtime/board-registry');
test('ePaper V2 is a conservative ESP board, without XIAO pins or LED',()=>{
 const d=boardDescriptors['waveshare_esp32s3_epaper_1.54_v2'];
 assert.ok(d,'missing ePaper V2 board');
 assert.deepEqual(d.board.exposedPins,[]);
 assert.equal(d.board.devices.display.width,200);
 assert.equal(d.board.devices.display.height,200);
 for(const f of ['gpio','spi','i2c','adc','pwm','neopixel','onboardLed']) assert.equal(d.features[f],false);
 assert.deepEqual(d.capabilities.usb.classes,['cdc','msc']);
});
