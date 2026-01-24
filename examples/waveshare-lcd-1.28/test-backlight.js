// test-backlight.js - Minimal test: just backlight
// If backlight doesn't turn on, it's a GPIO issue

console.log('=== Backlight Test ===');

// Backlight pin is GPIO 25
GPIO.init(25, GPIO.OUTPUT);

console.log('Turning backlight ON...');
GPIO.set(25, 1);
console.log('Backlight should be ON now');

// Toggle test
console.log('Toggling 3 times...');
for (var i = 0; i < 3; i++) {
  GPIO.set(25, 0);
  console.log('OFF');
  GPIO.set(25, 1);
  console.log('ON');
}

console.log('Final state: ON');
console.log('=== Test Complete ===');
