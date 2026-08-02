// Print the running board's immutable identity and capability registry.
// This example is diagnostic only and does not access hardware.
(function () {
    var boardApi = require('board');
    var modules = require('mcujs:module');
    var capabilities = boardApi.capabilities();
    var usb = boardApi.capability('usb');
    var image = boardApi.capability('image');

    console.log('Board:', boardApi.name);
    console.log('Chip:', boardApi.chip);
    console.log('Firmware:', boardApi.version);
    console.log('Portable API:', boardApi.apiVersion);
    console.log('Built-in modules:', JSON.stringify(modules.builtinModules));
    console.log('USB classes:', JSON.stringify(usb.classes));
    console.log('Keyboard module:', modules.has('keyboard') ? 'available' : 'unavailable');
    console.log('Mouse module:', modules.has('mouse') ? 'available' : 'unavailable');
    console.log('Image module:', modules.has('image') ? 'available' : 'unavailable');
    console.log('Image capability:', image ? JSON.stringify(image) : 'unavailable');
    console.log('Semantic pins:', JSON.stringify(boardApi.pins));
    console.log('Onboard devices:', JSON.stringify(boardApi.devices));
    console.log('Capabilities:', JSON.stringify(capabilities));
    console.log('Demo complete!');
}());
