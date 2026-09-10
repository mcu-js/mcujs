'use strict';

// Uncompressed 24-bit BMP, one row per timer turn. No full-file/string decode.
module.exports = function showBmp(path) {
  var fs = require('fs');
  var fd = fs.openSync(path, 'r');
  var display = null, timer = null, stopped = false;
  function closeFile() {
    if (fd !== null) { var closing = fd; fd = null; fs.closeSync(closing); }
  }
  function close() {
    stopped = true;
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
    var stride = Math.ceil(width * 3 / 4) * 4;
    if (header[0] !== 66 || header[1] !== 77 || v.getUint32(14, true) !== 40 ||
        v.getUint16(26, true) !== 1 || v.getUint16(28, true) !== 24 ||
        v.getUint32(30, true) !== 0 || width < 1 || width > 640 ||
        height < 1 || height > 640 || offset < 54 ||
        offset + stride * height > v.getUint32(2, true)) {
      throw new Error('Unsupported BMP: bounded 24-bit BI_RGB required');
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
        // Adjacent identical pixels share a rectangle; padding is never drawn.
        for (var x = 0; x < width;) {
          var i = x * 3, end = x + 1;
          while (end < width && row[end * 3] === row[i] &&
                 row[end * 3 + 1] === row[i + 1] && row[end * 3 + 2] === row[i + 2]) end++;
          ctx.fillStyle = '#' + hex(row[i + 2]) + hex(row[i + 1]) + hex(row[i]);
          if (rotated) ctx.fillRect(left + height - 1 - y, top + x, 1, end - x);
          else ctx.fillRect(left + x, top + y, end - x, 1);
          x = end;
        }
        y++;
        if (y < height) timer = setTimeout(step, 0);
        else {
          closeFile();
          console.log('BMP_READY ' + path + ' ' + width + 'x' + height + ' buffers=' + (54 + stride));
          timer = setTimeout(close, 60000);
        }
      } catch (error) {
        try { close(); } catch (ignored) {}
        console.log('BMP_ERROR ' + error.message);
      }
    }
    timer = setTimeout(step, 0);
    return { close: close };
  } catch (error) {
    try { close(); } catch (ignored) {}
    throw error;
  }
};
