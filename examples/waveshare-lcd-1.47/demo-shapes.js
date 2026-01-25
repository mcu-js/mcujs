// demo-shapes.js - Animated shapes demo
// For Waveshare RP2350-LCD-1.47-A (ST7789V3 320x172)

console.log('=== Shapes Demo ===');

// Screen setup
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

function fillCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    hline(cx-x,cy+y,2*x+1,c);hline(cx-x,cy-y,2*x+1,c);
    hline(cx-y,cy+x,2*y+1,c);hline(cx-y,cy-x,2*y+1,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
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

var frame = 0;

// Animated shapes
var rects = [];
var circles = [];
var lines = [];

// Initialize random shapes
for (var i = 0; i < 5; i++) {
  rects.push({
    x: Math.floor(Math.random() * (W - 60)) + 10,
    y: Math.floor(Math.random() * (H - 40)) + 10,
    w: Math.floor(Math.random() * 40) + 20,
    h: Math.floor(Math.random() * 30) + 15,
    vx: (Math.random() - 0.5) * 4,
    vy: (Math.random() - 0.5) * 3,
    c: rgb(Math.floor(Math.random() * 256), Math.floor(Math.random() * 256), Math.floor(Math.random() * 256))
  });
}

for (var i = 0; i < 4; i++) {
  circles.push({
    x: Math.floor(Math.random() * (W - 80)) + 40,
    y: Math.floor(Math.random() * (H - 60)) + 30,
    r: Math.floor(Math.random() * 20) + 15,
    vx: (Math.random() - 0.5) * 3,
    vy: (Math.random() - 0.5) * 2,
    c: rgb(Math.floor(Math.random() * 256), Math.floor(Math.random() * 256), Math.floor(Math.random() * 256))
  });
}

function animate() {
  // Dark gradient background
  fill(rgb(10, 10, 30));
  
  // Update and draw rectangles (outlined)
  for (var i = 0; i < rects.length; i++) {
    var r = rects[i];
    r.x += r.vx;
    r.y += r.vy;
    if (r.x < 0 || r.x + r.w > W) r.vx = -r.vx;
    if (r.y < 0 || r.y + r.h > H) r.vy = -r.vy;
    r.x = Math.max(0, Math.min(W - r.w, r.x));
    r.y = Math.max(0, Math.min(H - r.h, r.y));
    drawRect(Math.floor(r.x), Math.floor(r.y), r.w, r.h, r.c);
    drawRect(Math.floor(r.x)+1, Math.floor(r.y)+1, r.w-2, r.h-2, r.c);
  }
  
  // Update and draw circles (filled)
  for (var i = 0; i < circles.length; i++) {
    var c = circles[i];
    c.x += c.vx;
    c.y += c.vy;
    if (c.x - c.r < 0 || c.x + c.r > W) c.vx = -c.vx;
    if (c.y - c.r < 0 || c.y + c.r > H) c.vy = -c.vy;
    c.x = Math.max(c.r, Math.min(W - c.r, c.x));
    c.y = Math.max(c.r, Math.min(H - c.r, c.y));
    fillCircle(Math.floor(c.x), Math.floor(c.y), c.r, c.c);
  }
  
  // Draw rotating lines from center
  var cx = W / 2;
  var cy = H / 2;
  var angle = frame * 0.05;
  for (var i = 0; i < 6; i++) {
    var a = angle + i * 3.14159 / 3;
    var x1 = cx + Math.floor(Math.cos(a) * 50);
    var y1 = cy + Math.floor(Math.sin(a) * 40);
    var x2 = cx + Math.floor(Math.cos(a + 3.14159) * 50);
    var y2 = cy + Math.floor(Math.sin(a + 3.14159) * 40);
    var col = rgb(
      Math.floor(128 + 127 * Math.sin(angle + i)),
      Math.floor(128 + 127 * Math.sin(angle + i + 2)),
      Math.floor(128 + 127 * Math.sin(angle + i + 4))
    );
    drawLine(x1, y1, x2, y2, col);
  }
  
  // Center dot
  fillCircle(cx, cy, 5, WHITE);
  
  flush();
  frame++;
}

console.log('Starting animation...');
var animInterval = setInterval(animate, 16);  // ~60fps target

// Run for 20 seconds
setTimeout(function() {
  clearInterval(animInterval);
  console.log('Shapes demo complete!');
}, 20000);
