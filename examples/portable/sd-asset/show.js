'use strict';

// Install on /app and call require('/app/show')('/sd/your-asset.json').
// Pixel data comes only from the file; no board names, pins or embedded picture.
module.exports = function showAsset(path) {
  var raw = require('fs').readFileSync(path, 'utf8');
  var asset = JSON.parse(raw);
  if (!asset || !Number.isInteger(asset.width) || !Number.isInteger(asset.height) ||
      asset.width < 1 || asset.width > 32 || asset.height < 1 || asset.height > 32 ||
      !Array.isArray(asset.palette) || asset.palette.length < 1 || asset.palette.length > 10 ||
      typeof asset.pixels !== 'string' || asset.pixels.length !== asset.width * asset.height) {
    throw new Error('Invalid bounded pixel asset');
  }
  for (var i = 0; i < asset.palette.length; i++) {
    if (typeof asset.palette[i] !== 'string' || !/^(black|white|aqua|lime|yellow|red|blue|magenta)$/.test(asset.palette[i])) {
      throw new Error('Invalid asset palette');
    }
  }
  for (var p = 0; p < asset.pixels.length; p++) {
    if (asset.pixels.charCodeAt(p) < 48 || asset.pixels.charCodeAt(p) >= 48 + asset.palette.length) {
      throw new Error('Invalid asset pixel');
    }
  }
  var display = require('devices').display.open();
  try {
    var canvas = display.canvas, ctx = canvas.getContext('2d');
    var scale = Math.floor(Math.min(canvas.width / asset.width, canvas.height / asset.height));
    var left = Math.floor((canvas.width - asset.width * scale) / 2);
    var top = Math.floor((canvas.height - asset.height * scale) / 2);
    ctx.fillStyle = 'black'; ctx.fillRect(0, 0, canvas.width, canvas.height);
    for (var y = 0; y < asset.height; y++) {
      for (var x = 0; x < asset.width; x++) {
        ctx.fillStyle = asset.palette[Number(asset.pixels[y * asset.width + x])];
        ctx.fillRect(left + x * scale, top + y * scale, scale, scale);
      }
    }
    display.present();
    console.log('SD_ASSET_READY ' + path);
    // Raw readback lets the external verifier compare bytes, not echoed inputs.
    console.log('SD_ASSET_BYTES ' + raw);
    setTimeout(function () { display.close(); console.log('SD asset demo complete.'); }, 60000);
  } catch (error) { display.close(); throw error; }
};
