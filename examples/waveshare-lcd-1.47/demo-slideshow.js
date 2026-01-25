// demo-slideshow.js - Slideshow demo with text, shapes, and colors
// For Waveshare RP2350-LCD-1.47-A (ST7789V3 320x172)

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

// Screen setup - 320x172 horizontal
var W = 320, H = 172;
var ROW_OFFSET = 0x22;
var pins = { spiBus: 0, sck: 18, mosi: 19, cs: 17, dc: 16, rst: 20, bl: 21 };
var buf = graphics.createBuffer({ width: W, height: H });

function cmd(c) { GPIO.set(pins.dc, 0); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, c); GPIO.set(pins.cs, 1); }
function dat(d) { GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, d); GPIO.set(pins.cs, 1); }

function initScreen() {
  GPIO.init(pins.cs, GPIO.OUTPUT);
  GPIO.init(pins.dc, GPIO.OUTPUT);
  GPIO.init(pins.rst, GPIO.OUTPUT);
  GPIO.init(pins.bl, GPIO.OUTPUT);
  GPIO.set(pins.cs, 1); GPIO.set(pins.bl, 0); GPIO.set(pins.rst, 1);
  SPI.init(pins.spiBus, pins.sck, pins.mosi, 255, 40000000);
  
  GPIO.set(pins.rst, 1); board.delay(100);
  GPIO.set(pins.rst, 0); board.delay(100);
  GPIO.set(pins.rst, 1); board.delay(100);
  
  cmd(0x11); board.delay(120);
  cmd(0x36); dat(0x70);
  cmd(0x3A); dat(0x05);
  cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
  cmd(0xB7); dat(0x35);
  cmd(0xBB); dat(0x35);
  cmd(0xC0); dat(0x2C);
  cmd(0xC2); dat(0x01);
  cmd(0xC3); dat(0x13);
  cmd(0xC4); dat(0x20);
  cmd(0xC6); dat(0x0F);
  cmd(0xD0); dat(0xA4); dat(0xA1);
  cmd(0xD6); dat(0xA1);
  cmd(0xE0); dat(0xF0);dat(0x00);dat(0x04);dat(0x04);dat(0x04);dat(0x05);dat(0x29);dat(0x33);dat(0x3E);dat(0x38);dat(0x12);dat(0x12);dat(0x28);dat(0x30);
  cmd(0xE1); dat(0xF0);dat(0x07);dat(0x0A);dat(0x0D);dat(0x0B);dat(0x07);dat(0x28);dat(0x33);dat(0x3E);dat(0x36);dat(0x14);dat(0x14);dat(0x29);dat(0x32);
  cmd(0x21);
  cmd(0x11); board.delay(120);
  
  graphics.fill(buf, 0x0000);
  flush();
  
  cmd(0x29); board.delay(20);
  GPIO.set(pins.bl, 1);
}

function flush() {
  cmd(0x2A); dat(0x00); dat(0x00); dat((W-1)>>8); dat((W-1)&0xFF);
  cmd(0x2B); dat(0x00); dat(ROW_OFFSET); dat((H-1+ROW_OFFSET)>>8); dat((H-1+ROW_OFFSET)&0xFF);
  cmd(0x2C);
  GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0);
  SPI.writeBufferDMA(pins.spiBus, buf, W*H*2);
  GPIO.set(pins.cs, 1);
}

function rgb(r,g,b) { return graphics.color565(r,g,b); }
function fill(c) { graphics.fill(buf, c); }
function fillRect(x,y,w,h,c) { graphics.fillRect(buf, x, y, w, h, c); }
function setPixel(x,y,c) { graphics.setPixel(buf, x, y, c); }
function hline(x,y,w,c) { fillRect(x,y,w,1,c); }
function vline(x,y,h,c) { fillRect(x,y,1,h,c); }

function drawRect(x,y,w,h,c) {
  hline(x,y,w,c); hline(x,y+h-1,w,c);
  vline(x,y,h,c); vline(x+w-1,y,h,c);
}

function drawCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    setPixel(cx+x,cy+y,c);setPixel(cx+y,cy+x,c);
    setPixel(cx-y,cy+x,c);setPixel(cx-x,cy+y,c);
    setPixel(cx-x,cy-y,c);setPixel(cx-y,cy-x,c);
    setPixel(cx+y,cy-x,c);setPixel(cx+x,cy-y,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
}

function fillCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    hline(cx-x,cy+y,2*x+1,c);hline(cx-x,cy-y,2*x+1,c);
    hline(cx-y,cy+x,2*y+1,c);hline(cx-y,cy-x,2*y+1,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
}

function drawLine(x0,y0,x1,y1,c) {
  var dx=Math.abs(x1-x0),dy=Math.abs(y1-y0);
  var sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx-dy;
  while(true) {
    setPixel(x0,y0,c);
    if(x0===x1&&y0===y1)break;
    var e2=2*e;
    if(e2>-dy){e-=dy;x0+=sx;}
    if(e2<dx){e+=dx;y0+=sy;}
  }
}

function drawChar(ch, x, y, c, scale) {
  scale = scale || 1;
  var code = ch.charCodeAt(0);
  if (code < 32 || code > 126) code = 32;
  var idx = (code - 32) * 5;
  for (var col = 0; col < 5; col++) {
    var bits = font[idx + col];
    for (var row = 0; row < 7; row++) {
      if (bits & (1 << row)) {
        if (scale === 1) setPixel(x + col, y + row, c);
        else fillRect(x + col * scale, y + row * scale, scale, scale, c);
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
var BLACK = rgb(0,0,0);
var WHITE = rgb(255,255,255);
var RED = rgb(255,0,0);
var GREEN = rgb(0,255,0);
var BLUE = rgb(0,0,255);
var CYAN = rgb(0,255,255);
var MAGENTA = rgb(255,0,255);
var YELLOW = rgb(255,255,0);
var ORANGE = rgb(255,128,0);

// Initialize
initScreen();
console.log('Screen ready: ' + W + 'x' + H);

// Slide 1: Title
function slide1() {
  console.log('Slide 1: Title');
  fill(rgb(0,0,64));
  centerText('mcujs', 20, WHITE, 5);
  centerText('JavaScript for', 80, CYAN, 2);
  centerText('Microcontrollers', 105, CYAN, 2);
  centerText('RP2350 LCD 1.47 - 320x172', 145, YELLOW, 1);
  flush();
}

// Slide 2: Rectangles
function slide2() {
  console.log('Slide 2: Rectangles');
  fill(BLACK);
  centerText('RECTANGLES', 10, WHITE, 2);
  var colors = [RED,ORANGE,YELLOW,GREEN,CYAN,BLUE,MAGENTA];
  for (var i = 0; i < 7; i++) {
    var x = 20 + i * 8;
    var y = 35 + i * 8;
    drawRect(x, y, W - x*2, H - y - 10 - i*8, colors[i]);
    drawRect(x+1, y+1, W - x*2 - 2, H - y - 12 - i*8, colors[i]);
  }
  flush();
}

// Slide 3: Circles
function slide3() {
  console.log('Slide 3: Circles');
  fill(rgb(32,0,32));
  centerText('CIRCLES', 10, WHITE, 2);
  var cx = W / 2;
  var cy = 100;
  var colors = [CYAN,GREEN,YELLOW,ORANGE,RED,MAGENTA];
  for (var i = 0; i < 6; i++) {
    drawCircle(cx, cy, 65 - i*10, colors[i]);
    drawCircle(cx, cy, 64 - i*10, colors[i]);
  }
  fillCircle(cx, cy, 12, WHITE);
  flush();
}

// Slide 4: Lines - Starburst
function slide4() {
  console.log('Slide 4: Lines');
  fill(rgb(0,16,32));
  centerText('LINES', 10, WHITE, 2);
  var cx = W / 2;
  var cy = 100;
  for (var a = 0; a < 360; a += 10) {
    var rad = a * 3.14159 / 180;
    var x = cx + Math.floor(Math.cos(rad) * 70);
    var y = cy + Math.floor(Math.sin(rad) * 55);
    var c = rgb(
      Math.floor(128 + 127 * Math.cos(rad)),
      Math.floor(128 + 127 * Math.cos(rad - 2.09)),
      Math.floor(128 + 127 * Math.cos(rad - 4.19))
    );
    drawLine(cx, cy, x, y, c);
  }
  fillCircle(cx, cy, 10, WHITE);
  flush();
}

// Slide 5: Filled Shapes
function slide5() {
  console.log('Slide 5: Filled Shapes');
  fill(BLACK);
  centerText('FILLED SHAPES', 10, WHITE, 2);
  fillCircle(60, 95, 35, RED);
  fillCircle(160, 95, 35, GREEN);
  fillCircle(260, 95, 35, BLUE);
  fillRect(100, 140, 50, 25, YELLOW);
  fillRect(170, 140, 50, 25, CYAN);
  flush();
}

// Slide 6: Grid
function slide6() {
  console.log('Slide 6: Grid');
  fill(rgb(16,16,32));
  centerText('GRID', 10, CYAN, 2);
  for (var x = 20; x < W - 20; x += 20) vline(x, 35, H - 50, rgb(0,64,128));
  for (var y = 35; y < H - 15; y += 20) hline(20, y, W - 40, rgb(0,64,128));
  for (var gx = 40; gx <= W - 40; gx += 40) {
    for (var gy = 55; gy <= H - 35; gy += 40) {
      fillCircle(gx, gy, 4, CYAN);
    }
  }
  flush();
}

// Slide 7: Color Bars
function slide7() {
  console.log('Slide 7: Color Bars');
  fill(BLACK);
  centerText('COLOR BARS', 10, WHITE, 2);
  var colors = [RED, ORANGE, YELLOW, GREEN, CYAN, BLUE, MAGENTA, WHITE];
  var barW = Math.floor((W - 40) / 8);
  for (var i = 0; i < 8; i++) {
    fillRect(20 + i * barW, 40, barW - 2, H - 55, colors[i]);
  }
  flush();
}

// Slide 8: Checkerboard
function slide8() {
  console.log('Slide 8: Checkerboard');
  fill(BLACK);
  centerText('CHECKER', 5, YELLOW, 2);
  var sz = 20;
  var cols = Math.floor((W - 20) / sz);
  var rows = Math.floor((H - 40) / sz);
  for (var r = 0; r < rows; r++) {
    for (var c = 0; c < cols; c++) {
      if ((r + c) % 2 === 0) {
        fillRect(10 + c * sz, 30 + r * sz, sz, sz, WHITE);
      }
    }
  }
  flush();
}

// Slide 9: Text Sizes
function slide9() {
  console.log('Slide 9: Text');
  fill(rgb(0,32,0));
  drawText('Scale 1', 10, 15, WHITE, 1);
  drawText('Scale 2', 10, 35, YELLOW, 2);
  drawText('Scale 3', 10, 65, CYAN, 3);
  drawText('BIG!', 10, 110, RED, 5);
  drawText('RP2350', 200, 130, MAGENTA, 3);
  flush();
}

// Slide 10: Credits
function slide10() {
  console.log('Slide 10: Credits');
  fill(rgb(32,0,64));
  centerText('mcujs', 25, WHITE, 4);
  centerText('Graphics Demo', 75, CYAN, 2);
  centerText('320x172 RGB565 - DMA Transfer', 110, YELLOW, 1);
  centerText('Waveshare RP2350-LCD-1.47-A', 130, YELLOW, 1);
  fillCircle(100, 155, 12, RED);
  fillCircle(160, 155, 12, GREEN);
  fillCircle(220, 155, 12, BLUE);
  flush();
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
