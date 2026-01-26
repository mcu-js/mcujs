/*
 * Digital Clock Demo for Waveshare RP2040-PiZero
 * 
 * Displays a digital clock with animated seconds indicator.
 * Shows text rendering and simple animation on DVI output.
 * 
 * Note: Without RTC, this shows elapsed time from boot.
 * Run time: 60 seconds then exits cleanly
 */

// Initialize DVI display
var dvi = {
  width: DVI.width,
  height: DVI.height,
  byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(ptr, len) { DVI.show(ptr, len); }
};

screen.init(dvi);
console.log('Digital Clock Demo');

var w = screen.getWidth();
var h = screen.getHeight();

// Time tracking (simulated - starts at 00:00:00)
var hours = 0;
var minutes = 0;
var seconds = 0;
var frameCount = 0;

// Colors
var bgColor = screen.rgb(0, 0, 32);  // Dark blue
var textColor = screen.WHITE;
var accentColor = screen.CYAN;

function pad(n) {
  return n < 10 ? '0' + n : '' + n;
}

function drawClock() {
  // Clear with dark blue background
  screen.fill(bgColor);
  
  // Draw time string
  var timeStr = pad(hours) + ':' + pad(minutes) + ':' + pad(seconds);
  
  // Center the clock (each char is 6*3=18 pixels wide at size 3)
  var textWidth = timeStr.length * 18;
  var x = Math.floor((w - textWidth) / 2);
  var y = Math.floor(h / 2) - 10;
  
  screen.drawText(x, y, timeStr, textColor, 3);
  
  // Draw seconds progress bar
  var barWidth = 120;
  var barHeight = 8;
  var barX = Math.floor((w - barWidth) / 2);
  var barY = y + 30;
  
  // Background bar
  screen.fillRect(barX, barY, barWidth, barHeight, screen.GRAY);
  
  // Progress fill
  var progress = Math.floor((seconds / 60) * barWidth);
  if (progress > 0) {
    screen.fillRect(barX, barY, progress, barHeight, accentColor);
  }
  
  // Draw frame indicator dots (shows animation is running)
  var dotX = 10 + (frameCount % 20);
  screen.fillCircle(dotX, h - 10, 3, screen.GREEN);
  
  // Flush to display
  screen.show();
}

var running = true;
var totalSeconds = 0;

// Update every second
var intervalId = setInterval(function() {
  if (!running) return;
  
  // Draw current time
  drawClock();
  
  // Increment time
  seconds++;
  if (seconds >= 60) {
    seconds = 0;
    minutes++;
    if (minutes >= 60) {
      minutes = 0;
      hours++;
      if (hours >= 24) {
        hours = 0;
      }
    }
  }
  
  frameCount++;
  totalSeconds++;
  
  // Stop after 60 seconds
  if (totalSeconds >= 60) {
    running = false;
    clearInterval(intervalId);
    screen.fill(screen.BLACK);
    screen.drawText(20, 50, 'Demo complete!', screen.GREEN, 2);
    screen.show();
    console.log('Demo complete!');
  }
}, 1000);

// Initial draw
drawClock();
console.log('Running for 60 seconds...');
