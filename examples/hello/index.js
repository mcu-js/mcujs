// Copy to /app/hello.js and .run /app/hello.js. Stops after 30 seconds.
(function () {
    if (typeof globalThis.helloStop === 'function') globalThis.helloStop();
    var board = require('board');
    console.log('Hello from mcujs!');
    console.log('Board:', board.name);
    console.log('Chip:', board.chip);
    console.log('Free memory:', board.freeMemory(), 'bytes');
    var count = 0, interval, timer, done = false;
    function finish() {
        if (done) return;
        done = true;
        clearInterval(interval);
        clearTimeout(timer);
        console.log('Demo complete!');
    }
    globalThis.helloStop = finish;
    try {
        interval = setInterval(function () {
            count++;
            console.log('Heartbeat #' + count + ' - uptime: ' + board.millis() + 'ms');
        }, 5000);
        timer = setTimeout(finish, 30000);
    } catch (error) { finish(); throw error; }
    console.log('Hello World example running for 30 seconds; helloStop() stops early.');
}());
