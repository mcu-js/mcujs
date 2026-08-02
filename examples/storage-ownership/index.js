/*
 * Portable storage ownership check.
 * No board-name or pin assumptions.
 */
(function () {
  const board = require('board');
  const modules = require('mcujs:module');

  if (!modules.has('fs')) {
    console.log('This firmware has no filesystem.');
    return;
  }

  if (typeof board.storageReady !== 'function') {
    console.log('This firmware does not expose storage ownership state.');
    return;
  }

  if (!board.storageReady()) {
    console.log('Storage is owned by the USB host; eject the MCU.js volume first.');
    return;
  }

  try {
    const fs = require('fs');
    console.log('Device storage is ready:', fs.existsSync('/index.js'));
  } catch (error) {
    if (error && error.code === 'EBUSY') {
      console.log('Storage ownership changed; eject the MCU.js volume and retry.');
      return;
    }
    throw error;
  }
})();
