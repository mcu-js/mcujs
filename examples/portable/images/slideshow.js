'use strict';

// One pass through 1..8 explicit paths. No directory scan or display-specific API.
module.exports = function slideshow(paths) {
  if (!Array.isArray(paths) || paths.length < 1 || paths.length > 8)
    throw new RangeError('Supply 1..8 image paths');
  paths = paths.slice();
  for (var i = 0; i < paths.length; i++)
    if (typeof paths[i] !== 'string' || !paths[i].length) throw new TypeError('Image paths must be nonempty strings');
  var info = require('./info');
  var showJpeg = require('../jpeg/show'), showBmp = require('../sd-bmp/show');
  var index = 0, active = null, timer = null, deadline = null, stopped = false;
  function release() {
    if (timer !== null) clearTimeout(timer);
    timer = null;
    if (deadline !== null) clearTimeout(deadline);
    deadline = null;
    if (active) { var old = active; active = null; old.close(); }
  }
  function close() {
    stopped = true;
    if (timer !== null) clearTimeout(timer);
    timer = null;
    release();
  }
  function abort(error) {
    try { close(); } catch (ignored) {}
    console.log('SLIDESHOW_ERROR ' + error.message);
  }
  function schedule(fn, delay) {
    timer = setTimeout(function () {
      timer = null;
      if (stopped) return;
      try { fn(); } catch (error) { abort(error); }
    }, delay);
  }
  function failed(message) {
    console.log('SLIDE_ERROR ' + paths[index - 1] + ' ' + message);
    release();
    schedule(next, 0);
  }
  function check() {
    var state = active.status();
    if (state === 'loading') { schedule(check, 100); return; }
    if (state !== 'ready') { failed('render ' + state); return; }
    clearTimeout(deadline); deadline = null;
    console.log('SLIDE_READY ' + paths[index - 1]);
    schedule(next, 2000); // Hold only after the renderer has completed.
  }
  function next() {
    release();
    if (index === paths.length) {
      stopped = true;
      console.log('SLIDESHOW_DONE');
      return;
    }
    var file = paths[index++], metadata;
    try {
      metadata = info(file);
      console.log('IMAGE_INFO ' + file + ' ' + metadata.format + ' ' + metadata.width + 'x' + metadata.height);
      if (metadata.format !== 'jpeg' && metadata.format !== 'bmp') throw new Error('Unknown image format');
      active = (metadata.format === 'jpeg' ? showJpeg : showBmp)(file);
    } catch (error) { failed(error.message); return; }
    deadline = setTimeout(function () {
      deadline = null;
      if (stopped) return;
      try { failed('render timed out'); } catch (error) { abort(error); }
    }, 122000);
    schedule(check, 100);
  }
  try { schedule(next, 0); }
  catch (error) { close(); throw error; }
  return { close: close };
};
