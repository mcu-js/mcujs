// demo-fps.js - FPS Benchmark for Waveshare RP2350-Touch-LCD-1.69
// Measures frame rate with bouncing balls animation

var screen = require('./screen.js').createScreen();

screen.init();

var W = screen.width;
var H = screen.height;

// Animation state
var frame = 0;
var fps = 0;
var lastTime = Date.now();
var frameCount = 0;

// Bouncing balls
var balls = [
  { x: 60, y: 70, vx: 3, vy: 2, r: 18, c: screen.RED },
  { x: 120, y: 140, vx: -2, vy: 3, r: 15, c: screen.GREEN },
  { x: 180, y: 200, vx: 2, vy: -2, r: 12, c: screen.BLUE }
];

function animate() {
  // Clear
  screen.fill(screen.rgb(0, 0, 32));
  
  // Update and draw balls
  for (var i = 0; i < balls.length; i++) {
    var b = balls[i];
    b.x += b.vx;
    b.y += b.vy;
    
    // Bounce off edges
    if (b.x - b.r < 0 || b.x + b.r > W) b.vx = -b.vx;
    if (b.y - b.r < 0 || b.y + b.r > H) b.vy = -b.vy;
    
    // Draw ball
    screen.fillCircle(Math.floor(b.x), Math.floor(b.y), b.r, b.c);
  }
  
  // Flush to display
  screen.flush();
  
  // Calculate FPS
  frameCount++;
  frame++;
  var now = Date.now();
  if (now - lastTime >= 1000) {
    fps = frameCount;
    frameCount = 0;
    lastTime = now;
    console.log('FPS: ' + fps);
  }
}

console.log('FPS Benchmark - Bouncing balls');
console.log('Running for 15 seconds...');

var animInterval = setInterval(animate, 1);

setTimeout(function() {
  clearInterval(animInterval);
  console.log('Final FPS: ' + fps);
  console.log('Demo complete!');
}, 15000);
