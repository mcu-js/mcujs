'use strict';

// Bounded replacement for the old pin-wired image feature diagnostic.
module.exports = function features(jpegPath, iconPath) {
  var info = require('./info'), jpeg = require('../jpeg/show'), bmp = require('../sd-bmp/show');
  var background = info(jpegPath), icon = info(iconPath);
  if (background.format !== 'jpeg' || icon.format !== 'bmp' || icon.width !== 32 || icon.height !== 32)
    throw new Error('Supply a JPEG background and a 32x32 BMP icon');
  var display = null, active = null, timer = null, deadline = null, stopped = false;
  var scene = 0, index = 0, jobs, canvas, ctx, w, h, cx, cy;
  var names = ['corners', 'center', 'clipping', 'overlay'];
  function release() {
    var pending = timer, expiry = deadline;
    timer = null; deadline = null;
    try { if (pending !== null) clearTimeout(pending); } finally {
      try { if (expiry !== null) clearTimeout(expiry); } finally {
        try { if (active) { var task = active; active = null; task.close(); } } finally {
          if (display) { var old = display; display = null; old.close(); }
        }
      }
    }
  }
  function close() { stopped = true; release(); }
  function failed(error) {
    try { close(); } catch (ignored) {}
    console.log('IMAGE_FEATURE_ERROR ' + error.message);
  }
  function later(fn, delay) {
    timer = setTimeout(function () {
      timer = null;
      if (stopped) return;
      try { fn(); } catch (error) { failed(error); }
    }, delay);
  }
  function rect(color, x, y, width, height) {
    ctx.fillStyle = color; ctx.fillRect(x, y, width, height);
  }
  function step() {
    if (active) {
      var state = active.status();
      if (state === 'loading') { later(step, 100); return; }
      if (state !== 'ready') throw new Error('Image render ' + state);
      var old = active; active = null; old.close();
    }
    if (index < jobs.length) {
      var xy = jobs[index++];
      active = scene === 3 && index === 1
        ? jpeg.draw(jpegPath, canvas, xy[0], xy[1])
        : bmp.draw(iconPath, canvas, xy[0], xy[1]);
      later(step, 100); return;
    }
    if (scene === 3) {
      rect('white', 10, 10, 50, 50);
      rect('black', w - 60, 10, 50, 50);
      rect('yellow', 0, Math.floor(h / 2), w, 2);
    }
    display.present();
    console.log('IMAGE_FEATURE_READY ' + names[scene]);
    scene++;
    if (scene < 4) later(start, 2000);
    else {
      clearTimeout(deadline); deadline = null;
      later(close, 60000);
      console.log('IMAGE_FEATURES_DONE');
    }
  }
  function start() {
    index = 0;
    rect(['blue', 'red', 'lime', 'black'][scene], 0, 0, w, h);
    if (scene === 0) jobs = [[0,0],[w-32,0],[0,h-32],[w-32,h-32]];
    else if (scene === 1) jobs = [[cx,cy]];
    else if (scene === 2) jobs = [[-16,cy],[w-16,cy],[cx,-16],[cx,h-16]];
    else {
      // Center without rotating/scaling; oversized backgrounds clip to Canvas.
      jobs = [[Math.floor((w-background.width)/2), Math.floor((h-background.height)/2)]];
      for (var i = 0; i < 5; i++) jobs.push([Math.floor(i*(w-32)/4), Math.min(h-32, Math.floor(h/2)+20+(i%2)*20)]);
    }
    step();
  }
  try {
    display = require('devices').display.open(); canvas = display.canvas;
    w = canvas.width; h = canvas.height;
    if (w < 128 || h < 128) throw new Error('Feature diagnostic needs a Canvas at least 128x128');
    cx = Math.floor((w-32)/2); cy = Math.floor((h-32)/2); ctx = canvas.getContext('2d');
    deadline = setTimeout(function () { if (!stopped) failed(new Error('Image features timed out')); }, 180000);
    later(start, 0);
    return { close: close };
  } catch (error) { try { close(); } catch (ignored) {} throw error; }
};
