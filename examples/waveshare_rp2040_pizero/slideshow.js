/*
 * Slideshow Demo for Waveshare RP2040-PiZero
 * Shows various graphics capabilities with auto-advancing slides.
 * Run time: 30 seconds
 */

var dvi = {
  width: DVI.width, height: DVI.height, byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(p, l) { DVI.show(p, l); }
};
screen.init(dvi);
console.log('Slideshow Demo');

var W = screen.getWidth();
var H = screen.getHeight();

function rgb(r, g, b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

var BLACK = 0, WHITE = 0xFFFF;
var RED = rgb(255,0,0), GREEN = rgb(0,255,0), BLUE = rgb(0,0,255);
var CYAN = rgb(0,255,255), MAGENTA = rgb(255,0,255), YELLOW = rgb(255,255,0);
var ORANGE = rgb(255,128,0);

// Slide 1: Title
function slide1() {
  console.log('Slide 1: Title');
  screen.fill(rgb(0,0,64));
  screen.drawText(35, 20, 'mcujs', WHITE, 3);
  screen.drawText(30, 55, 'JavaScript', CYAN, 1);
  screen.drawText(35, 70, 'for MCUs', CYAN, 1);
  screen.drawText(5, 95, 'RP2040-PiZero DVI', YELLOW, 1);
  screen.show();
}

// Slide 2: Shapes
function slide2() {
  console.log('Slide 2: Shapes');
  screen.fill(BLACK);
  screen.drawText(40, 5, 'SHAPES', WHITE, 1);
  screen.fillCircle(40, 60, 20, RED);
  screen.fillCircle(80, 60, 20, GREEN);
  screen.fillCircle(120, 60, 20, BLUE);
  screen.fillRect(20, 90, 35, 20, YELLOW);
  screen.fillRect(65, 90, 35, 20, CYAN);
  screen.fillRect(110, 90, 35, 20, MAGENTA);
  screen.show();
}

// Slide 3: Color Bars
function slide3() {
  console.log('Slide 3: Colors');
  screen.fill(BLACK);
  screen.drawText(30, 5, 'COLOR BARS', WHITE, 1);
  var colors = [RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA, WHITE];
  var barW = 18;
  for (var i = 0; i < 8; i++) {
    screen.fillRect(6 + i * barW, 18, barW - 2, 95, colors[i]);
  }
  screen.show();
}

// Slide 4: Checkerboard
function slide4() {
  console.log('Slide 4: Checker');
  screen.fill(BLACK);
  screen.drawText(35, 5, 'CHECKER', YELLOW, 1);
  var sz = 12;
  for (var r = 0; r < 8; r++) {
    for (var c = 0; c < 13; c++) {
      if ((r + c) % 2 === 0) {
        screen.fillRect(4 + c * sz, 18 + r * sz, sz, sz, WHITE);
      }
    }
  }
  screen.show();
}

// Slide 5: Concentric circles
function slide5() {
  console.log('Slide 5: Circles');
  screen.fill(rgb(32, 0, 32));
  screen.drawText(35, 5, 'CIRCLES', WHITE, 1);
  var cx = 80, cy = 65;
  var colors = [RED, ORANGE, YELLOW, GREEN, CYAN, BLUE];
  for (var i = 0; i < 6; i++) {
    screen.fillCircle(cx, cy, 40 - i * 6, colors[i]);
  }
  screen.fillCircle(cx, cy, 8, WHITE);
  screen.show();
}

// Slide 6: Grid
function slide6() {
  console.log('Slide 6: Grid');
  screen.fill(rgb(0, 16, 32));
  screen.drawText(50, 5, 'GRID', CYAN, 1);
  for (var x = 10; x < W; x += 15) {
    screen.fillRect(x, 18, 1, H - 25, rgb(0, 64, 128));
  }
  for (var y = 18; y < H - 5; y += 15) {
    screen.fillRect(10, y, W - 20, 1, rgb(0, 64, 128));
  }
  screen.show();
}

// Slide 7: Text sizes
function slide7() {
  console.log('Slide 7: Text');
  screen.fill(rgb(0, 32, 0));
  screen.drawText(5, 10, 'Size 1', WHITE, 1);
  screen.drawText(5, 30, 'Size 2', YELLOW, 2);
  screen.drawText(5, 60, 'BIG', CYAN, 3);
  screen.drawText(70, 80, 'DVI!', RED, 3);
  screen.show();
}

// Slide 8: Credits
function slide8() {
  console.log('Slide 8: Credits');
  screen.fill(rgb(32, 0, 64));
  screen.drawText(40, 15, 'mcujs', WHITE, 2);
  screen.drawText(30, 45, 'DVI Demo', CYAN, 1);
  screen.drawText(15, 65, '160x120 RGB565', YELLOW, 1);
  screen.drawText(5, 80, '4x scaled 640x480', YELLOW, 1);
  screen.fillCircle(50, 105, 8, RED);
  screen.fillCircle(80, 105, 8, GREEN);
  screen.fillCircle(110, 105, 8, BLUE);
  screen.show();
}

var slides = [slide1, slide2, slide3, slide4, slide5, slide6, slide7, slide8];
var idx = 0;

function next() {
  slides[idx]();
  idx = (idx + 1) % slides.length;
}

next();
console.log('Auto-advance every 3 seconds...');

var timer = setInterval(next, 3000);

setTimeout(function() {
  clearInterval(timer);
  screen.fill(BLACK);
  screen.drawText(30, 50, 'Done!', GREEN, 2);
  screen.show();
  console.log('Demo complete!');
}, 30000);
