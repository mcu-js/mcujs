// demo-fps.js - FPS benchmark for Waveshare RP2350-LCD-1.47-A
// Measures frames per second with different drawing operations
// Display: ST7789V3 320x172 (horizontal orientation)

console.log('=== FPS Demo ===');

// 5x7 font (digits only for speed)
var font = [
  0x3E,0x51,0x49,0x45,0x3E, // 0
  0x00,0x42,0x7F,0x40,0x00, // 1
  0x42,0x61,0x51,0x49,0x46, // 2
  0x21,0x41,0x45,0x4B,0x31, // 3
  0x18,0x14,0x12,0x7F,0x10, // 4
  0x27,0x45,0x45,0x45,0x39, // 5
  0x3C,0x4A,0x49,0x49,0x30, // 6
  0x01,0x71,0x09,0x05,0x03, // 7
  0x36,0x49,0x49,0x49,0x36, // 8
  0x06,0x49,0x49,0x29,0x1E  // 9
];

// Screen setup - 320x172 horizontal
var W = 320, H = 172;
var ROW_OFFSET = 0x22;  // ST7789 row offset
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
  
  // Hardware reset
  GPIO.set(pins.rst, 1); board.delay(100);
  GPIO.set(pins.rst, 0); board.delay(100);
  GPIO.set(pins.rst, 1); board.delay(100);
  
  // ST7789 init sequence
  cmd(0x11); board.delay(120);  // Sleep out
  cmd(0x36); dat(0x70);         // Memory access: horizontal
  cmd(0x3A); dat(0x05);         // 16-bit color
  cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);  // Porch
  cmd(0xB7); dat(0x35);         // Gate control
  cmd(0xBB); dat(0x35);         // VCOM
  cmd(0xC0); dat(0x2C);         // LCM
  cmd(0xC2); dat(0x01);         // VDV/VRH enable
  cmd(0xC3); dat(0x13);         // VRH
  cmd(0xC4); dat(0x20);         // VDV
  cmd(0xC6); dat(0x0F);         // Frame rate
  cmd(0xD0); dat(0xA4); dat(0xA1);  // Power
  cmd(0xD6); dat(0xA1);
  cmd(0xE0); dat(0xF0);dat(0x00);dat(0x04);dat(0x04);dat(0x04);dat(0x05);dat(0x29);dat(0x33);dat(0x3E);dat(0x38);dat(0x12);dat(0x12);dat(0x28);dat(0x30);
  cmd(0xE1); dat(0xF0);dat(0x07);dat(0x0A);dat(0x0D);dat(0x0B);dat(0x07);dat(0x28);dat(0x33);dat(0x3E);dat(0x36);dat(0x14);dat(0x14);dat(0x29);dat(0x32);
  cmd(0x21);  // Inversion on
  cmd(0x11); board.delay(120);
  
  // Clear before display on
  graphics.fill(buf, 0x0000);
  flush();
  
  cmd(0x29); board.delay(20);  // Display on
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

function drawDigit(d, x, y, c, scale) {
  var idx = d * 5;
  for (var col = 0; col < 5; col++) {
    var bits = font[idx + col];
    for (var row = 0; row < 7; row++) {
      if (bits & (1 << row)) {
        fillRect(x + col * scale, y + row * scale, scale, scale, c);
      }
    }
  }
}

function drawNumber(n, x, y, c, scale) {
  var str = String(n);
  for (var i = 0; i < str.length; i++) {
    var d = str.charCodeAt(i) - 48;
    if (d >= 0 && d <= 9) {
      drawDigit(d, x + i * 6 * scale, y, c, scale);
    }
  }
}

function hline(x,y,w,c) { fillRect(x,y,w,1,c); }

function fillCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    hline(cx-x,cy+y,2*x+1,c);hline(cx-x,cy-y,2*x+1,c);
    hline(cx-y,cy+x,2*y+1,c);hline(cx-y,cy-x,2*y+1,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
}

// Colors
var BLACK = rgb(0,0,0);
var WHITE = rgb(255,255,255);
var RED = rgb(255,0,0);
var GREEN = rgb(0,255,0);
var BLUE = rgb(0,0,255);
var YELLOW = rgb(255,255,0);
var CYAN = rgb(0,255,255);

// Initialize
initScreen();
console.log('Screen ready: ' + W + 'x' + H);

// Animation state
var frame = 0;
var fps = 0;
var lastTime = Date.now();
var frameCount = 0;

// Bouncing balls
var balls = [
  { x: 80, y: 86, vx: 3, vy: 2, r: 18, c: RED },
  { x: 160, y: 60, vx: -2, vy: 3, r: 15, c: GREEN },
  { x: 240, y: 100, vx: 2, vy: -2, r: 12, c: BLUE }
];

// Animation loop
function animate() {
  // Clear with dark blue
  fill(rgb(0, 0, 32));
  
  // Update and draw balls
  for (var i = 0; i < balls.length; i++) {
    var b = balls[i];
    b.x += b.vx;
    b.y += b.vy;
    
    // Bounce off edges
    if (b.x - b.r < 0 || b.x + b.r > W) b.vx = -b.vx;
    if (b.y - b.r < 0 || b.y + b.r > H) b.vy = -b.vy;
    
    // Draw ball with highlight
    fillCircle(b.x, b.y, b.r, b.c);
    fillCircle(b.x - 4, b.y - 4, b.r - 6, rgb(255, 200, 200));
    fillCircle(b.x - 5, b.y - 5, 4, WHITE);
  }
  
  // Draw moving bars at top and bottom
  var offset = frame % 80;
  fillRect(offset, 5, 60, 10, CYAN);
  fillRect(W - 80 - offset, 5, 60, 10, YELLOW);
  fillRect(offset, H - 15, 60, 10, YELLOW);
  fillRect(W - 80 - offset, H - 15, 60, 10, CYAN);
  
  // Draw FPS counter in center
  fillRect(125, 65, 70, 42, rgb(0, 0, 64));
  drawNumber(fps, 135, 75, WHITE, 3);
  
  // Flush to display
  flush();
  
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

// Run animation
console.log('Starting animation...');
var animInterval = setInterval(animate, 1);

// Run for 15 seconds
setTimeout(function() {
  clearInterval(animInterval);
  console.log('Final FPS: ' + fps);
  console.log('Demo complete!');
}, 15000);
