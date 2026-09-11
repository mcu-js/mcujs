'use strict';

// RGB565 BITFIELDS or 24/32-bit BI_RGB BMP, one row per timer turn.
// No full-file/string decode and no display-specific pixel packing.
module.exports = function showBmp(path) {
  var fs = require('fs');
  var fd = fs.openSync(path, 'r');
  var display = null, timer = null, stopped = false, state = 'loading';
  function closeFile() {
    if (fd !== null) { var closing = fd; fd = null; fs.closeSync(closing); }
  }
  function close() {
    stopped = true;
    if (state !== 'error') state = 'closed';
    if (timer !== null) clearTimeout(timer);
    try { closeFile(); } finally {
      if (display) { var d = display; display = null; d.close(); }
    }
  }
  function readExact(buffer, position) {
    var done = 0;
    while (done < buffer.length) {
      var n = fs.readSync(fd, buffer, done, buffer.length - done, position + done);
      if (!n) throw new Error('Truncated BMP');
      done += n;
    }
  }
  function hex(n) { return (256 + n).toString(16).slice(1); }
  try {
    var header = new Uint8Array(54);
    readExact(header, 0);
    var v = new DataView(header.buffer);
    var offset = v.getUint32(10, true), width = v.getInt32(18, true);
    var signedHeight = v.getInt32(22, true), height = Math.abs(signedHeight);
    var bits = v.getUint16(28, true), compression = v.getUint32(30, true);
    var bytes = bits / 8, stride = Math.ceil(width * bytes / 4) * 4;
    if (header[0] !== 66 || header[1] !== 77 || v.getUint32(14, true) !== 40 ||
        v.getUint16(26, true) !== 1 ||
        !((bits === 16 && compression === 3) || ((bits === 24 || bits === 32) && compression === 0)) ||
        width < 1 || width > 640 ||
        height < 1 || height > 640 || offset < 54 ||
        (bits === 16 && offset < 66) ||
        offset + stride * height > v.getUint32(2, true)) {
      throw new Error('Unsupported BMP: bounded RGB565 BITFIELDS or 24/32-bit BI_RGB required');
    }
    var masks = null;
    if (bits === 16) {
      masks = new Uint8Array(12); readExact(masks, 54);
      var mv = new DataView(masks.buffer);
      if (mv.getUint32(0, true) !== 0xf800 || mv.getUint32(4, true) !== 0x07e0 ||
          mv.getUint32(8, true) !== 0x001f) throw new Error('Unsupported BMP: RGB565 masks required');
    }
    var row = new Uint8Array(stride);
    display = require('devices').display.open();
    var canvas = display.canvas;
    var rotated = width > canvas.width || height > canvas.height;
    if (rotated && (height > canvas.width || width > canvas.height)) throw new Error('BMP exceeds display');
    var ctx = canvas.getContext('2d');
    var left = Math.floor((canvas.width - (rotated ? height : width)) / 2);
    var top = Math.floor((canvas.height - (rotated ? width : height)) / 2);
    ctx.fillStyle = 'black'; ctx.fillRect(0, 0, canvas.width, canvas.height);
    var y = 0;
    function step() {
      if (stopped) return;
      try {
        readExact(row, offset + (signedHeight < 0 ? y : height - 1 - y) * stride);
        // Adjacent identical colors share a rectangle; padding is never drawn.
        // The fourth BI_RGB byte is unused, not Canvas transparency.
        for (var x = 0; x < width;) {
          var i = x * bytes, end = x + 1;
          while (end < width && row[end * bytes] === row[i] &&
                 row[end * bytes + 1] === row[i + 1] &&
                 (bits === 16 || row[end * bytes + 2] === row[i + 2])) end++;
          var r, g, b;
          if (bits === 16) {
            var pixel = row[i] | (row[i + 1] << 8);
            r = pixel >> 11; g = (pixel >> 5) & 63; b = pixel & 31;
            r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
          } else { r = row[i + 2]; g = row[i + 1]; b = row[i]; }
          ctx.fillStyle = '#' + hex(r) + hex(g) + hex(b);
          if (rotated) ctx.fillRect(left + height - 1 - y, top + x, 1, end - x);
          else ctx.fillRect(left + x, top + y, end - x, 1);
          x = end;
        }
        y++;
        if (y < height) timer = setTimeout(step, 0);
        else {
          closeFile();
          display.present();
          state = 'ready';
          timer = setTimeout(close, 60000);
          console.log('BMP_READY ' + path + ' ' + width + 'x' + height + ' buffers=' + (54 + stride + (masks ? 12 : 0)));
        }
      } catch (error) {
        state = 'error';
        try { close(); } catch (ignored) {}
        console.log('BMP_ERROR ' + error.message);
      }
    }
    timer = setTimeout(step, 0);
    return { close: close, status: function () { return state; } };
  } catch (error) {
    try { close(); } catch (ignored) {}
    throw error;
  }
};
