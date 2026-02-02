/*
 * JPEG Slideshow for Waveshare RP2040-PiZero
 * 
 * Displays JPEG images from the /slides/ directory on the DVI output.
 * Resolution: 160x120 framebuffer, scaled 4x to 640x480 HDMI output.
 * 
 * Run time: 60 seconds
 * 
 * Usage:
 *   require('/jpeg-slideshow.js')
 * 
 * Requirements:
 * - Create a /slides/ directory on the device filesystem
 * - Copy 160x120 JPEG images (max 16KB each) to /slides/
 * - Use baseline DCT JPEG format (not progressive)
 * 
 * Example image preparation (using ImageMagick):
 *   magick input.jpg -resize 160x120! -quality 70 slide1.jpg
 */

var fs = require('fs');
var image = require('image');

var running = false;
var slideTimer = 0;
var doneTimer = 0;

function stopTimers() {
  if (slideTimer) {
    clearInterval(slideTimer);
    slideTimer = 0;
  }
  if (doneTimer) {
    clearTimeout(doneTimer);
    doneTimer = 0;
  }
}

function startSlideshow() {
  if (running) {
    stopTimers();
  }
  // Initialize DVI first so we can use the draw buffer directly
  if (!DVI.isRunning()) {
    DVI.init();
    DVI.start();
  }

  // Initialize screen with DVI driver
  var dvi = {
    width: DVI.width,
    height: DVI.height,
    byteOrder: 'native',
    buffer: DVI.getDrawBuffer(),
    init: function() {},
    show: function() {
      DVI.swapAndShow();
      this.buffer = DVI.getDrawBuffer();
    }
  };
  screen.init(dvi);

  console.log('JPEG Slideshow Demo');
  console.log('Resolution: ' + screen.getWidth() + 'x' + screen.getHeight() + ' -> 640x480 HDMI');

  // Get buffer handle and byte order from screen (no manual pointer/size needed!)
  var buffer = screen.getBufferHandle();
  var byteOrder = screen.getByteOrder();

  // Clear screen
  screen.fill(screen.BLACK);
  screen.show();

  // Load image list from /slides/ directory
  var slides = [];
  var slideDir = '/slides';

  try {
    var files = fs.readdirSync(slideDir);
    for (var i = 0; i < files.length; i++) {
      var f = files[i];
      var lower = f.toLowerCase();
      if (lower.indexOf('.jpg') >= 0 || lower.indexOf('.jpeg') >= 0) {
        slides.push(slideDir + '/' + f);
      }
    }
    console.log('Found ' + slides.length + ' JPEG files in ' + slideDir);
  } catch (e) {
    console.log('Error reading ' + slideDir + ': ' + e.message);
    console.log('');
    console.log('Setup instructions:');
    console.log('1. Create /slides/ directory on device');
    console.log('2. Copy 160x120 JPEGs to /slides/');
    console.log('   (max 16KB each, baseline DCT)');
  }

  // If no slides found, show a placeholder message
  if (slides.length === 0) {
    screen.fill(screen.BLUE);
    screen.fillRect(60, 50, 40, 20, screen.WHITE);
    screen.show();
    console.log('No slides found - showing placeholder');
    console.log('Demo complete!');
    running = false;
    return;
  }

  // Slideshow loop
  var currentIndex = 0;
  var slideInterval = 5000; // 5 seconds per slide
  
  function showSlide() {
    var path = slides[currentIndex];
    console.log('Showing: ' + path + ' (' + (currentIndex + 1) + '/' + slides.length + ')');
    
    try {
      // Refresh handle in case DVI swapped buffers since last frame
      buffer = screen.getBufferHandle();
      byteOrder = screen.getByteOrder();
      var t0 = board.millis();
      // Use screen's buffer handle and byte order - no manual pointer math!
      image.drawJPEG(buffer, path, { x: 0, y: 0, byteOrder: byteOrder });
      var elapsed = board.millis() - t0;
      console.log('Decoded in ' + elapsed + 'ms');
      screen.show();
    } catch (e) {
      console.log('Error loading ' + path + ': ' + e.message);
      screen.fill(screen.RED);
      screen.show();
    }
    
    currentIndex = (currentIndex + 1) % slides.length;
  }
  
  // Show first slide immediately
  showSlide();
  
  // Auto-advance every 5 seconds
  console.log('Auto-advance every ' + (slideInterval / 1000) + ' seconds...');
  slideTimer = setInterval(showSlide, slideInterval);
  running = true;
  
  // Exit after 60 seconds
  doneTimer = setTimeout(function() {
    stopTimers();
    screen.fill(screen.BLACK);
    screen.show();
    console.log('Demo complete!');
    running = false;
  }, 60000);
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { start: startSlideshow };
} else {
  startSlideshow();
}
