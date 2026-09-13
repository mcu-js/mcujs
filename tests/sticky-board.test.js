const {test}=require('node:test');
const assert=require('node:assert/strict');
const {readFileSync}=require('node:fs');
const {boardDescriptors,shippingBoardIds}=require('../runtime/board-registry');
const read=p=>readFileSync(require('node:path').join(__dirname,'..',p),'utf8');
test('Sticky is a conservative UART-bridge board, not a native USB board',()=>{
 const d=boardDescriptors.seeed_reterminal_sticky;
 assert.ok(d,'missing Sticky board');
 assert.deepEqual(d.board.exposedPins,[]);assert.deepEqual(d.board.pins,{});
 assert.equal(d.board.devices.display.controller,'SSD1677');
 assert.equal(d.board.devices.display.width,800);assert.equal(d.board.devices.display.height,480);
 for(const f of ['gpio','spi','i2c','adc','pwm','neopixel','onboardLed','onboardButton','image','screen','graphics']) assert.equal(d.features[f],false);
 assert.deepEqual(d.capabilities.usb.classes,[]);
 assert.equal(d.capabilities.fs.appRoot,'/app');
 assert.equal(d.capabilities.fs.writable,true);
 assert.equal(d.capabilities.fs.hostTransfer,false);
 assert.equal(d.capabilities.fs.sd.writable,false);
 assert.equal(d.capabilities.fs.sd.root,'/sd');
 assert.equal(d.features.safeMode,true);assert.ok(d.modules.includes('fs'));
 assert.ok(!shippingBoardIds.includes('seeed_reterminal_sticky'));
});
test('Sticky build selects UART only and packages bin, not UF2',()=>{
 const cmake=read('platform/esp32/main/CMakeLists.txt');
 assert.match(cmake,/list\(REMOVE_ITEM MCUJS_SOURCES usb_cdc.c usb_msc.c\)/);
 assert.match(cmake,/list\(APPEND MCUJS_SOURCES serial_uart.c\)/);
 assert.doesNotMatch(read('platform/esp32/main/serial_uart.c'),/tinyusb_driver_install|tud_task/);
 assert.match(read('platform/esp32/docker-entrypoint.sh'),/seeed_reterminal_sticky/);
 const config=read('platform/esp32/sdkconfig.sticky');
 for(const line of ['CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y','CONFIG_SPIRAM_MODE_OCT=y','CONFIG_SPIRAM_BOOT_INIT=y','# CONFIG_TINYUSB_CDC_ENABLED is not set','# CONFIG_TINYUSB_MSC_ENABLED is not set']) assert.ok(config.includes(line));
});

test('Sticky advertises only read-only SD assets, independently of app flash',()=>{
 const fs=JSON.parse(read('runtime/manifests/seeed_reterminal_sticky.json')).capabilities.fs;
 assert.equal(fs.appRoot,'/app');assert.equal(fs.writable,true);
 assert.deepEqual(fs.sd,{root:'/sd',implementation:'fat',writable:false,hostTransfer:false,removable:true,formats:['fat16','fat32']});
});

test('Sticky metadata validates without inventing native USB or another controller',()=>{
 const {validatePortableApiManifest:validate}=require('../scripts/validate-portable-api-manifest');
 const result=validate(JSON.parse(read('runtime/manifests/seeed_reterminal_sticky.json')));
 assert.equal(result.valid,true,JSON.stringify(result.errors));
});
