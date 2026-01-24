// test-screen.js - Test the screen factory composition pattern
// This is the cleanest API for web developers!

console.log('=== Screen Factory Test ===');

var screenModule = require('./screen');
console.log('Screen module loaded');

// Create screen with default driver
var screen = screenModule.createScreen();
console.log('Screen created:', screen.width, 'x', screen.height);

// Initialize (turns on display and backlight)
screen.init();
console.log('Screen initialized');

// Use chainable API
screen
  .fill(screen.BLUE)
  .flush();
console.log('Blue screen');

screen
  .fill(screen.RED)
  .flush();
console.log('Red screen');

screen
  .fill(screen.GREEN)
  .flush();
console.log('Green screen');

// Draw some shapes using color helpers
screen
  .fill(screen.rgb(20, 20, 40))  // Dark blue background
  .fillRect(20, 20, 60, 60, screen.RED)
  .fillRect(80, 60, 60, 60, screen.GREEN)
  .fillRect(140, 100, 60, 60, screen.BLUE)
  .fillRect(60, 140, 120, 40, screen.YELLOW)
  .fillRect(100, 100, 40, 40, screen.WHITE)
  .flush();
console.log('Colored shapes drawn');

// Draw a gradient-like pattern
screen.fill(screen.BLACK);
for (var y = 0; y < 240; y += 10) {
  var intensity = Math.floor((y / 240) * 255);
  var color = screen.rgb(intensity, 0, 255 - intensity);
  screen.fillRect(0, y, 240, 10, color);
}
screen.flush();
console.log('Gradient pattern');

// Cleanup
screen.destroy();
console.log('Screen destroyed');

console.log('=== Screen Factory Test COMPLETE ===');
