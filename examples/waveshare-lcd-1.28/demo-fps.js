// demo-fps.js - FPS benchmark for Waveshare LCD 1.28
// Measures frames per second with different drawing operations

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

// Screen setup
var W = 240, H = 240;
var pins = { spiBus: 1, sck: 10, mosi: 11, miso: 12, cs: 9, dc: 8, rst: 13, bl: 25 };
var buf = graphics.createBuffer({ width: W, height: H });

function cmd(c) { GPIO.set(pins.dc, 0); SPI.transfer(pins.spiBus, c); }
function dat(d) { GPIO.set(pins.dc, 1); SPI.transfer(pins.spiBus, d); }

function initScreen() {
  GPIO.init(pins.cs, GPIO.OUTPUT);
  GPIO.init(pins.dc, GPIO.OUTPUT);
  GPIO.init(pins.rst, GPIO.OUTPUT);
  GPIO.init(pins.bl, GPIO.OUTPUT);
  GPIO.set(pins.cs, 1); GPIO.set(pins.bl, 0); GPIO.set(pins.rst, 1);
  SPI.init(pins.spiBus, pins.sck, pins.mosi, pins.miso, 40000000);
  GPIO.set(pins.rst, 1); board.delay(100);
  GPIO.set(pins.rst, 0); board.delay(100);
  GPIO.set(pins.rst, 1); GPIO.set(pins.cs, 0); board.delay(100);
  cmd(0xEF);cmd(0xEB);dat(0x14);cmd(0xFE);cmd(0xEF);cmd(0xEB);dat(0x14);
  cmd(0x84);dat(0x40);cmd(0x85);dat(0xFF);cmd(0x86);dat(0xFF);cmd(0x87);dat(0xFF);
  cmd(0x88);dat(0x0A);cmd(0x89);dat(0x21);cmd(0x8A);dat(0x00);cmd(0x8B);dat(0x80);
  cmd(0x8C);dat(0x01);cmd(0x8D);dat(0x01);cmd(0x8E);dat(0xFF);cmd(0x8F);dat(0xFF);
  cmd(0xB6);dat(0x00);dat(0x20);cmd(0x36);dat(0x08);cmd(0x3A);dat(0x05);
  cmd(0x90);dat(0x08);dat(0x08);dat(0x08);dat(0x08);cmd(0xBD);dat(0x06);cmd(0xBC);dat(0x00);
  cmd(0xFF);dat(0x60);dat(0x01);dat(0x04);cmd(0xC3);dat(0x13);cmd(0xC4);dat(0x13);
  cmd(0xC9);dat(0x22);cmd(0xBE);dat(0x11);cmd(0xE1);dat(0x10);dat(0x0E);
  cmd(0xDF);dat(0x21);dat(0x0c);dat(0x02);
  cmd(0xF0);dat(0x45);dat(0x09);dat(0x08);dat(0x08);dat(0x26);dat(0x2A);
  cmd(0xF1);dat(0x43);dat(0x70);dat(0x72);dat(0x36);dat(0x37);dat(0x6F);
  cmd(0xF2);dat(0x45);dat(0x09);dat(0x08);dat(0x08);dat(0x26);dat(0x2A);
  cmd(0xF3);dat(0x43);dat(0x70);dat(0x72);dat(0x36);dat(0x37);dat(0x6F);
  cmd(0xED);dat(0x1B);dat(0x0B);cmd(0xAE);dat(0x77);cmd(0xCD);dat(0x63);
  cmd(0x70);dat(0x07);dat(0x07);dat(0x04);dat(0x0E);dat(0x0F);dat(0x09);dat(0x07);dat(0x08);dat(0x03);
  cmd(0xE8);dat(0x34);
  cmd(0x62);dat(0x18);dat(0x0D);dat(0x71);dat(0xED);dat(0x70);dat(0x70);dat(0x18);dat(0x0F);dat(0x71);dat(0xEF);dat(0x70);dat(0x70);
  cmd(0x63);dat(0x18);dat(0x11);dat(0x71);dat(0xF1);dat(0x70);dat(0x70);dat(0x18);dat(0x13);dat(0x71);dat(0xF3);dat(0x70);dat(0x70);
  cmd(0x64);dat(0x28);dat(0x29);dat(0xF1);dat(0x01);dat(0xF1);dat(0x00);dat(0x07);
  cmd(0x66);dat(0x3C);dat(0x00);dat(0xCD);dat(0x67);dat(0x45);dat(0x45);dat(0x10);dat(0x00);dat(0x00);dat(0x00);
  cmd(0x67);dat(0x00);dat(0x3C);dat(0x00);dat(0x00);dat(0x00);dat(0x01);dat(0x54);dat(0x10);dat(0x32);dat(0x98);
  cmd(0x74);dat(0x10);dat(0x85);dat(0x80);dat(0x00);dat(0x00);dat(0x4E);dat(0x00);
  cmd(0x98);dat(0x3e);dat(0x07);cmd(0x35);cmd(0x21);cmd(0x11);
  board.delay(120);
  graphics.fill(buf, 0x0000);
  cmd(0x2A);dat(0x00);dat(0x00);dat(0x00);dat(W-1);
  cmd(0x2B);dat(0x00);dat(0x00);dat(0x00);dat(H-1);
  cmd(0x2C);GPIO.set(pins.dc, 1);
  SPI.writeBufferDMA(pins.spiBus, buf, W*H*2);
  cmd(0x29); board.delay(20);
  GPIO.set(pins.bl, 1);
}

function flush() {
  cmd(0x2A);dat(0x00);dat(0x00);dat(0x00);dat(W-1);
  cmd(0x2B);dat(0x00);dat(0x00);dat(0x00);dat(H-1);
  cmd(0x2C);GPIO.set(pins.dc, 1);
  SPI.writeBufferDMA(pins.spiBus, buf, W*H*2);
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

// Initialize
initScreen();
console.log('Screen ready');

// Animation state
var frame = 0;
var fps = 0;
var lastTime = Date.now();
var frameCount = 0;

// Bouncing ball state
var ballX = 120, ballY = 120;
var ballVX = 3, ballVY = 2;
var ballR = 20;

// Animation loop
function animate() {
  // Clear
  fill(rgb(0, 0, 32));
  
  // Update ball position
  ballX += ballVX;
  ballY += ballVY;
  
  // Bounce off edges (accounting for circular display)
  if (ballX - ballR < 30 || ballX + ballR > 210) ballVX = -ballVX;
  if (ballY - ballR < 30 || ballY + ballR > 210) ballVY = -ballVY;
  
  // Draw ball with gradient effect
  fillCircle(ballX, ballY, ballR, RED);
  fillCircle(ballX - 5, ballY - 5, ballR - 8, rgb(255, 100, 100));
  fillCircle(ballX - 7, ballY - 7, 5, WHITE);
  
  // Draw some moving rectangles
  var offset = frame % 60;
  fillRect(40 + offset, 40, 20, 20, GREEN);
  fillRect(180 - offset, 40, 20, 20, BLUE);
  fillRect(40 + offset, 180, 20, 20, YELLOW);
  fillRect(180 - offset, 180, 20, 20, rgb(255, 0, 255));
  
  // Draw FPS counter
  fillRect(85, 100, 70, 40, rgb(0, 0, 64));
  drawNumber(fps, 95, 110, WHITE, 3);
  
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
