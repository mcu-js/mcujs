// demo-imu.js - IMU Bubble Level Demo for Waveshare RP2350-Touch-LCD-1.69
// Shows a bubble that moves based on device tilt

var screen = require('./screen.js').createScreen();
var imu = require('./qmi8658.js').createIMUDriver();

screen.init();
imu.init();

var centerX = 120;
var centerY = 140;
var bubbleRadius = 20;
var maxOffset = 80;

console.log('IMU Bubble Level Demo');
console.log('Tilt the device to move the bubble');
console.log('Running for 30 seconds...');

function loop() {
  var data = imu.readAccel();
  
  // Map accelerometer to bubble position
  // X tilt moves bubble horizontally, Y tilt moves vertically
  var bubbleX = centerX + Math.floor(data.x * maxOffset);
  var bubbleY = centerY + Math.floor(data.y * maxOffset);
  
  // Clamp to screen bounds
  bubbleX = Math.max(bubbleRadius, Math.min(screen.width - bubbleRadius, bubbleX));
  bubbleY = Math.max(bubbleRadius, Math.min(screen.height - bubbleRadius, bubbleY));
  
  // Draw
  screen.clear();
  
  // Draw target circle in center
  screen.drawCircle(centerX, centerY, 30, screen.GRAY);
  screen.drawCircle(centerX, centerY, 60, screen.GRAY);
  
  // Draw crosshairs
  screen.drawLine(centerX - 70, centerY, centerX + 70, centerY, screen.GRAY);
  screen.drawLine(centerX, centerY - 70, centerX, centerY + 70, screen.GRAY);
  
  // Draw bubble
  var bubbleColor = screen.GREEN;
  var dist = Math.sqrt((bubbleX - centerX) * (bubbleX - centerX) + (bubbleY - centerY) * (bubbleY - centerY));
  if (dist > 60) {
    bubbleColor = screen.RED;
  } else if (dist > 30) {
    bubbleColor = screen.YELLOW;
  }
  
  screen.fillCircle(bubbleX, bubbleY, bubbleRadius, bubbleColor);
  screen.drawCircle(bubbleX, bubbleY, bubbleRadius, screen.WHITE);
  
  screen.flush();
}

var interval = setInterval(loop, 30);

setTimeout(function() {
  clearInterval(interval);
  console.log('Demo complete!');
}, 30000);
