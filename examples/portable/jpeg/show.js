'use strict';

// Baseline JPEG through the existing Canvas API; no graphics/display handle in decoding.
// One <=256-byte file read or one <=16x16 RGB block per timer turn.
module.exports = function showJpeg(path) {
  var fs = require('fs'), jpeg = require('jpeg');
  if (typeof jpeg.open !== 'function') throw new Error('This firmware needs the JPEG reader');
  var fd = null, reader = null, display = null, input = null, pixels = null;
  var timer = null, deadline = null, stopped = false, ready = false;
  function closeFile() {
    if (fd !== null) { var old = fd; fd = null; fs.closeSync(old); }
  }
  function closeReader() {
    if (reader) { var old = reader; reader = null; old.close(); }
  }
  function close() {
    stopped = true;
    if (timer !== null) clearTimeout(timer);
    if (deadline !== null) clearTimeout(deadline);
    input = null; pixels = null;
    try { closeFile(); } finally {
      try { closeReader(); } finally {
        if (display) { var old = display; display = null; old.close(); }
      }
    }
  }
  function failed(error) {
    try { close(); } catch (ignored) {}
    console.log('JPEG_ERROR ' + error.message);
  }
  function hex(n) { return (256 + n).toString(16).slice(1); }
  try {
    var size = fs.statSync(path).size;
    if (size < 1 || size > 16384 || size !== Math.floor(size)) throw new RangeError('JPEG input must be 1..16384 bytes');
    fd = fs.openSync(path, 'r');
    input = new Uint8Array(size);
    var position = 0, width, height, rotated, left, top, ctx;
    function decode() {
      if (stopped) return;
      try {
        var block = reader.read(pixels);
        if (!block) {
          closeReader(); pixels = null;
          display.present(); ready = true;
          console.log('JPEG_READY ' + path + ' ' + width + 'x' + height);
          return;
        }
        for (var y = 0; y < block.height; y++) {
          for (var x = 0; x < block.width;) {
            var i = (y * block.width + x) * 3, end = x + 1;
            while (end < block.width) {
              var j = (y * block.width + end) * 3;
              if (pixels[j] !== pixels[i] || pixels[j + 1] !== pixels[i + 1] || pixels[j + 2] !== pixels[i + 2]) break;
              end++;
            }
            ctx.fillStyle = '#' + hex(pixels[i]) + hex(pixels[i + 1]) + hex(pixels[i + 2]);
            if (rotated) ctx.fillRect(left + height - 1 - block.y - y, top + block.x + x, 1, end - x);
            else ctx.fillRect(left + block.x + x, top + block.y + y, end - x, 1);
            x = end;
          }
        }
        timer = setTimeout(decode, 0);
      } catch (error) { failed(error); }
    }
    function load() {
      if (stopped) return;
      try {
        if (position < size) {
          var n = fs.readSync(fd, input, position, Math.min(256, size - position), position);
          if (!n) throw new Error('Truncated JPEG file');
          position += n;
          timer = setTimeout(load, 0);
          return;
        }
        // Detect file growth without a second large allocation or an unbounded read.
        if (fs.readSync(fd, input, 0, 1, size)) throw new Error('JPEG file grew during reading');
        closeFile();
        reader = jpeg.open(input); input = null;
        width = reader.width; height = reader.height;
        pixels = new Uint8Array(768);
        display = require('devices').display.open();
        var canvas = display.canvas;
        rotated = width > canvas.width || height > canvas.height;
        if (rotated && (height > canvas.width || width > canvas.height)) throw new Error('JPEG exceeds display');
        left = Math.floor((canvas.width - (rotated ? height : width)) / 2);
        top = Math.floor((canvas.height - (rotated ? width : height)) / 2);
        ctx = canvas.getContext('2d');
        ctx.fillStyle = 'black'; ctx.fillRect(0, 0, canvas.width, canvas.height);
        timer = setTimeout(decode, 0);
      } catch (error) { failed(error); }
    }
    // Full-size Canvas rendering can exceed 60 seconds on the 1.47-inch LCD.
    deadline = setTimeout(function () {
      if (ready) close();
      else failed(new Error('JPEG timed out'));
    }, 120000);
    timer = setTimeout(load, 0);
    return { close: close };
  } catch (error) {
    try { close(); } catch (ignored) {}
    throw error;
  }
};
