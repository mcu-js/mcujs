// demo-slideshow.js - Slideshow demo with text, shapes, and colors
// For Waveshare RP2350-Touch-LCD-1.69 (ST7789V2 240x280)

console.log('=== Slideshow Demo ===');

// 5x7 bitmap font data (ASCII 32-126)
var FONT_W = 5, FONT_H = 7;
var font = [
  0x00,0x00,0x00,0x00,0x00, // Space
  0x00,0x00,0x5F,0x00,0x00, // !
  0x00,0x07,0x00,0x07,0x00, // "
  0x14,0x7F,0x14,0x7F,0x14, // #
  0x24,0x2A,0x7F,0x2A,0x12, // $
  0x23,0x13,0x08,0x64,0x62, // %
  0x36,0x49,0x55,0x22,0x50, // &
  0x00,0x05,0x03,0x00,0x00, // '
  0x00,0x1C,0x22,0x41,0x00, // (
  0x00,0x41,0x22,0x1C,0x00, // )
  0x08,0x2A,0x1C,0x2A,0x08, // *
  0x08,0x08,0x3E,0x08,0x08, // +
  0x00,0x50,0x30,0x00,0x00, // ,
  0x08,0x08,0x08,0x08,0x08, // -
  0x00,0x60,0x60,0x00,0x00, // .
  0x20,0x10,0x08,0x04,0x02, // /
  0x3E,0x51,0x49,0x45,0x3E, // 0
  0x00,0x42,0x7F,0x40,0x00, // 1
  0x42,0x61,0x51,0x49,0x46, // 2
  0x21,0x41,0x45,0x4B,0x31, // 3
  0x18,0x14,0x12,0x7F,0x10, // 4
  0x27,0x45,0x45,0x45,0x39, // 5
  0x3C,0x4A,0x49,0x49,0x30, // 6
  0x01,0x71,0x09,0x05,0x03, // 7
  0x36,0x49,0x49,0x49,0x36, // 8
  0x06,0x49,0x49,0x29,0x1E, // 9
  0x00,0x36,0x36,0x00,0x00, // :
  0x00,0x56,0x36,0x00,0x00, // ;
  0x00,0x08,0x14,0x22,0x41, // <
  0x14,0x14,0x14,0x14,0x14, // =
  0x41,0x22,0x14,0x08,0x00, // >
  0x02,0x01,0x51,0x09,0x06, // ?
  0x32,0x49,0x79,0x41,0x3E, // @
  0x7E,0x11,0x11,0x11,0x7E, // A
  0x7F,0x49,0x49,0x49,0x36, // B
  0x3E,0x41,0x41,0x41,0x22, // C
  0x7F,0x41,0x41,0x22,0x1C, // D
  0x7F,0x49,0x49,0x49,0x41, // E
  0x7F,0x09,0x09,0x01,0x01, // F
  0x3E,0x41,0x41,0x51,0x32, // G
  0x7F,0x08,0x08,0x08,0x7F, // H
  0x00,0x41,0x7F,0x41,0x00, // I
  0x20,0x40,0x41,0x3F,0x01, // J
  0x7F,0x08,0x14,0x22,0x41, // K
  0x7F,0x40,0x40,0x40,0x40, // L
  0x7F,0x02,0x04,0x02,0x7F, // M
  0x7F,0x04,0x08,0x10,0x7F, // N
  0x3E,0x41,0x41,0x41,0x3E, // O
  0x7F,0x09,0x09,0x09,0x06, // P
  0x3E,0x41,0x51,0x21,0x5E, // Q
  0x7F,0x09,0x19,0x29,0x46, // R
  0x46,0x49,0x49,0x49,0x31, // S
  0x01,0x01,0x7F,0x01,0x01, // T
  0x3F,0x40,0x40,0x40,0x3F, // U
  0x1F,0x20,0x40,0x20,0x1F, // V
  0x7F,0x20,0x18,0x20,0x7F, // W
  0x63,0x14,0x08,0x14,0x63, // X
  0x03,0x04,0x78,0x04,0x03, // Y
  0x61,0x51,0x49,0x45,0x43, // Z
  0x00,0x00,0x7F,0x41,0x41, // [
  0x02,0x04,0x08,0x10,0x20, // backslash
  0x41,0x41,0x7F,0x00,0x00, // ]
  0x04,0x02,0x01,0x02,0x04, // ^
  0x40,0x40,0x40,0x40,0x40, // _
  0x00,0x01,0x02,0x04,0x00, // `
  0x20,0x54,0x54,0x54,0x78, // a
  0x7F,0x48,0x44,0x44,0x38, // b
  0x38,0x44,0x44,0x44,0x20, // c
  0x38,0x44,0x44,0x48,0x7F, // d
  0x38,0x54,0x54,0x54,0x18, // e
  0x08,0x7E,0x09,0x01,0x02, // f
  0x08,0x14,0x54,0x54,0x3C, // g
  0x7F,0x08,0x04,0x04,0x78, // h
  0x00,0x44,0x7D,0x40,0x00, // i
  0x20,0x40,0x44,0x3D,0x00, // j
  0x00,0x7F,0x10,0x28,0x44, // k
  0x00,0x41,0x7F,0x40,0x00, // l
  0x7C,0x04,0x18,0x04,0x78, // m
  0x7C,0x08,0x04,0x04,0x78, // n
  0x38,0x44,0x44,0x44,0x38, // o
  0x7C,0x14,0x14,0x14,0x08, // p
  0x08,0x14,0x14,0x18,0x7C, // q
  0x7C,0x08,0x04,0x04,0x08, // r
  0x48,0x54,0x54,0x54,0x20, // s
  0x04,0x3F,0x44,0x40,0x20, // t
  0x3C,0x40,0x40,0x20,0x7C, // u
  0x1C,0x20,0x40,0x20,0x1C, // v
  0x3C,0x40,0x30,0x40,0x3C, // w
  0x44,0x28,0x10,0x28,0x44, // x
  0x0C,0x50,0x50,0x50,0x3C, // y
  0x44,0x64,0x54,0x4C,0x44, // z
  0x00,0x08,0x36,0x41,0x00, // {
  0x00,0x00,0x7F,0x00,0x00, // |
  0x00,0x41,0x36,0x08,0x00, // }
  0x08,0x04,0x08,0x10,0x08  // ~
];

// Use the screen module
var screen = require('./screen.js').createScreen();
screen.init();

var W = screen.width;   // 240
var H = screen.height;  // 280

// Drawing helpers
function drawChar(ch, x, y, c, scale) {
  scale = scale || 1;
  var code = ch.charCodeAt(0);
  if (code < 32 || code > 126) code = 32;
  var idx = (code - 32) * 5;
  for (var col = 0; col < 5; col++) {
    var bits = font[idx + col];
    for (var row = 0; row < 7; row++) {
      if (bits & (1 << row)) {
        if (scale === 1) {
          screen.setPixel(x + col, y + row, c);
        } else {
          screen.fillRect(x + col * scale, y + row * scale, scale, scale, c);
        }
      }
    }
  }
}

function drawText(str, x, y, c, scale) {
  scale = scale || 1;
  var spacing = (FONT_W + 1) * scale;
  for (var i = 0; i < str.length; i++) {
    drawChar(str.charAt(i), x + i * spacing, y, c, scale);
  }
}

function centerText(str, y, c, scale) {
  scale = scale || 1;
  var tw = str.length * (FONT_W + 1) * scale;
  drawText(str, Math.floor((W - tw) / 2), y, c, scale);
}

// Colors
var BLACK = screen.BLACK;
var WHITE = screen.WHITE;
var RED = screen.RED;
var GREEN = screen.GREEN;
var BLUE = screen.BLUE;
var CYAN = screen.CYAN;
var MAGENTA = screen.MAGENTA;
var YELLOW = screen.YELLOW;
var ORANGE = screen.ORANGE;

console.log('Screen ready: ' + W + 'x' + H);

// Slide 1: Title
function slide1() {
  console.log('Slide 1: Title');
  screen.fill(screen.rgb(0, 0, 64));
  centerText('mcujs', 40, WHITE, 5);
  centerText('JavaScript for', 110, CYAN, 2);
  centerText('Microcontrollers', 135, CYAN, 2);
  centerText('Touch LCD 1.69', 200, YELLOW, 2);
  centerText('240x280 RGB565', 240, YELLOW, 1);
  screen.flush();
}

// Slide 2: Rectangles
function slide2() {
  console.log('Slide 2: Rectangles');
  screen.fill(BLACK);
  centerText('RECTANGLES', 15, WHITE, 2);
  var colors = [RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA];
  for (var i = 0; i < 7; i++) {
    var o = 30 + i * 12;
    screen.drawRect(o, 45 + i * 12, W - o * 2, H - 60 - i * 24, colors[i]);
  }
  screen.flush();
}

// Slide 3: Circles
function slide3() {
  console.log('Slide 3: Circles');
  screen.fill(screen.rgb(32, 0, 32));
  centerText('CIRCLES', 15, WHITE, 2);
  var cx = W / 2;
  var cy = 160;
  var colors = [CYAN, GREEN, YELLOW, ORANGE, RED, MAGENTA];
  for (var i = 0; i < 6; i++) {
    screen.drawCircle(cx, cy, 100 - i * 15, colors[i]);
  }
  screen.fillCircle(cx, cy, 15, WHITE);
  screen.flush();
}

// Slide 4: Lines - Starburst
function slide4() {
  console.log('Slide 4: Lines');
  screen.fill(screen.rgb(0, 16, 32));
  centerText('LINES', 15, WHITE, 2);
  var cx = W / 2;
  var cy = 160;
  for (var a = 0; a < 360; a += 12) {
    var rad = a * 3.14159 / 180;
    var x = cx + Math.floor(Math.cos(rad) * 90);
    var y = cy + Math.floor(Math.sin(rad) * 90);
    var c = screen.rgb(
      Math.floor(128 + 127 * Math.cos(rad)),
      Math.floor(128 + 127 * Math.cos(rad - 2.09)),
      Math.floor(128 + 127 * Math.cos(rad - 4.19))
    );
    screen.drawLine(cx, cy, x, y, c);
  }
  screen.fillCircle(cx, cy, 12, WHITE);
  screen.flush();
}

// Slide 5: Filled Shapes
function slide5() {
  console.log('Slide 5: Filled Shapes');
  screen.fill(BLACK);
  centerText('FILLED', 15, WHITE, 2);
  screen.fillCircle(60, 100, 40, RED);
  screen.fillCircle(180, 100, 40, GREEN);
  screen.fillCircle(60, 200, 40, BLUE);
  screen.fillCircle(180, 200, 40, YELLOW);
  screen.fillRect(90, 130, 60, 60, WHITE);
  screen.flush();
}

// Slide 6: Grid
function slide6() {
  console.log('Slide 6: Grid');
  screen.fill(screen.rgb(16, 16, 32));
  centerText('GRID', 15, CYAN, 2);
  var gridColor = screen.rgb(0, 64, 128);
  for (var x = 20; x < W - 10; x += 20) {
    screen.drawVLine(x, 45, H - 60, gridColor);
  }
  for (var y = 45; y < H - 15; y += 20) {
    screen.drawHLine(20, y, W - 40, gridColor);
  }
  for (var gx = 40; gx <= W - 40; gx += 40) {
    for (var gy = 65; gy <= H - 35; gy += 40) {
      screen.fillCircle(gx, gy, 5, CYAN);
    }
  }
  screen.flush();
}

// Slide 7: Color Bars
function slide7() {
  console.log('Slide 7: Color Bars');
  screen.fill(BLACK);
  centerText('COLORS', 15, WHITE, 2);
  var colors = [RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA, WHITE];
  var barW = Math.floor((W - 20) / 8);
  for (var i = 0; i < 8; i++) {
    screen.fillRect(10 + i * barW, 50, barW - 2, H - 70, colors[i]);
  }
  screen.flush();
}

// Slide 8: Checkerboard
function slide8() {
  console.log('Slide 8: Checkerboard');
  screen.fill(BLACK);
  centerText('CHECKER', 10, YELLOW, 2);
  var sz = 28;
  var cols = Math.floor(W / sz);
  var rows = Math.floor((H - 50) / sz);
  for (var r = 0; r < rows; r++) {
    for (var c = 0; c < cols; c++) {
      if ((r + c) % 2 === 0) {
        screen.fillRect(c * sz, 40 + r * sz, sz, sz, WHITE);
      }
    }
  }
  screen.flush();
}

// Slide 9: Text Sizes
function slide9() {
  console.log('Slide 9: Text');
  screen.fill(screen.rgb(0, 32, 0));
  drawText('Scale 1', 10, 20, WHITE, 1);
  drawText('Scale 2', 10, 45, YELLOW, 2);
  drawText('Scale 3', 10, 80, CYAN, 3);
  drawText('BIG!', 10, 130, RED, 5);
  drawText('RP2350', 60, 200, MAGENTA, 3);
  drawText('Touch!', 70, 240, GREEN, 2);
  screen.flush();
}

// Slide 10: Credits
function slide10() {
  console.log('Slide 10: Credits');
  screen.fill(screen.rgb(32, 0, 64));
  centerText('mcujs', 30, WHITE, 4);
  centerText('Graphics Demo', 90, CYAN, 2);
  centerText('240x280 RGB565', 140, YELLOW, 1);
  centerText('DMA Transfer', 160, YELLOW, 1);
  centerText('Touch + IMU', 180, YELLOW, 1);
  centerText('+ Buzzer!', 200, YELLOW, 1);
  screen.fillCircle(60, 250, 15, RED);
  screen.fillCircle(120, 250, 15, GREEN);
  screen.fillCircle(180, 250, 15, BLUE);
  screen.flush();
}

// Run slideshow
var slides = [slide1, slide2, slide3, slide4, slide5, slide6, slide7, slide8, slide9, slide10];
var idx = 0;

function next() {
  slides[idx]();
  idx = (idx + 1) % slides.length;
}

// Show first slide
next();

// Auto-advance every 3 seconds
console.log('Auto-advancing every 3 seconds...');
var timer = setInterval(next, 3000);

// Stop after 30 seconds
setTimeout(function() {
  clearInterval(timer);
  console.log('Slideshow complete!');
}, 30000);
