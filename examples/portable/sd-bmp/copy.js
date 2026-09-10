'use strict';

// Copy a scoped asset in bounded timer turns; never overwrite an existing path.
module.exports = function copyAsset(source, target) {
  var fs = require('fs');
  if (fs.existsSync(target)) throw new Error('Target already exists');
  var input = null, output = null, timer = null, stopped = false, total = 0;
  var buffer = new Uint8Array(1024);
  function closeFiles() {
    try {
      if (output !== null) { var o = output; output = null; fs.closeSync(o); }
    } finally {
      if (input !== null) { var i = input; input = null; fs.closeSync(i); }
    }
  }
  function close() {
    stopped = true;
    if (timer !== null) clearTimeout(timer);
    closeFiles();
  }
  function step() {
    if (stopped) return;
    try {
      var n = fs.readSync(input, buffer, 0, buffer.length, null);
      if (!n) {
        closeFiles(); stopped = true;
        console.log('COPY_READY ' + target + ' bytes=' + total + ' buffer=1024');
        return;
      }
      var done = 0;
      while (done < n) {
        var written = fs.writeSync(output, buffer, done, n - done, null);
        if (!written) throw new Error('Copy made no write progress');
        done += written;
      }
      total += n;
      timer = setTimeout(step, 0);
    } catch (error) {
      try { close(); } catch (ignored) {}
      console.log('COPY_ERROR ' + error.code + ' ' + error.message);
    }
  }
  try {
    input = fs.openSync(source, 'r');
    output = fs.openSync(target, 'w');
    timer = setTimeout(step, 0);
    return { close: close };
  } catch (error) {
    try { close(); } catch (ignored) {}
    throw error;
  }
};
