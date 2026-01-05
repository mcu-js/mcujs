// Hello World Example for mcujs
// Copy this file to your Pico as index.js

console.log("Hello from mcujs!");
console.log("Board:", board.name);
console.log("Chip:", board.chip);
console.log("Free memory:", board.freeMemory(), "bytes");

// Print a message every 5 seconds
let count = 0;
setInterval(() => {
    count++;
    console.log(`Heartbeat #${count} - uptime: ${board.millis()}ms`);
}, 5000);

console.log("Hello World example running...");
