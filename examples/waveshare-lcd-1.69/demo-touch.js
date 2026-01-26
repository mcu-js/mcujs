// demo-touch.js - Touch Drawing Demo for Waveshare RP2350-Touch-LCD-1.69
// Draw on the screen with your finger

var screen = require('./lib/waveshare-lcd-1.69/screen.js').createScreen();
var touch = require('./lib/waveshare-lcd-1.69/cst816s.js').createTouchDriver();

screen.init();
touch.init();

// Start with black screen
screen.clear().flush();

var brushSize = 8;
var brushColor = screen.WHITE;
var colors = [screen.WHITE, screen.RED, screen.GREEN, screen.BLUE, screen.YELLOW, screen.CYAN, screen.MAGENTA];
var colorIndex = 0;

console.log('Touch Drawing Demo');
console.log('Draw on screen - swipe left/right to change color');
console.log('Double tap to clear');
console.log('Running for 60 seconds...');

var lastX = -1;
var lastY = -1;

function loop() {
  var t = touch.read();
  
  if (t.touched) {
    // Handle gestures
    if (t.gesture === touch.GESTURE_DOUBLE_TAP) {
      screen.clear().flush();
      console.log('Cleared!');
    } else if (t.gesture === touch.GESTURE_SWIPE_LEFT) {
      colorIndex = (colorIndex + 1) % colors.length;
      brushColor = colors[colorIndex];
      console.log('Color changed');
    } else if (t.gesture === touch.GESTURE_SWIPE_RIGHT) {
      colorIndex = (colorIndex - 1 + colors.length) % colors.length;
      brushColor = colors[colorIndex];
      console.log('Color changed');
    } else {
      // Draw at touch position
      screen.fillCircle(t.x, t.y, brushSize, brushColor);
      
      // Draw line from last position for smooth strokes
      if (lastX >= 0 && lastY >= 0) {
        screen.drawLine(lastX, lastY, t.x, t.y, brushColor);
      }
      
      screen.flush();
      lastX = t.x;
      lastY = t.y;
    }
  } else {
    lastX = -1;
    lastY = -1;
  }
}

var interval = setInterval(loop, 10);

setTimeout(function() {
  clearInterval(interval);
  console.log('Demo complete!');
}, 60000);
