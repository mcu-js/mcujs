// demo-rainbow.js - Animated rainbow/color demo
// For Waveshare RP2350-LCD-1.47-A (ST7789V3 320x172)

console.log('=== Rainbow Demo ===');

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

function fillCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    hline(cx-x,cy+y,2*x+1,c);hline(cx-x,cy-y,2*x+1,c);
    hline(cx-y,cy+x,2*y+1,c);hline(cx-y,cy-x,2*y+1,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
}

// HSV to RGB conversion (h: 0-360, s/v: 0-1)
function hsvToRgb(h, s, v) {
  var r, g, b;
  var i = Math.floor(h / 60) % 6;
  var f = h / 60 - Math.floor(h / 60);
  var p = v * (1 - s);
  var q = v * (1 - f * s);
  var t = v * (1 - (1 - f) * s);
  
  switch (i) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
  
  return rgb(Math.floor(r * 255), Math.floor(g * 255), Math.floor(b * 255));
}

// Initialize
initScreen();
console.log('Screen ready: ' + W + 'x' + H);

var frame = 0;
var mode = 0;

// Mode 1: Horizontal rainbow bars (scrolling)
function rainbowBars() {
  var offset = frame * 2;
  for (var x = 0; x < W; x++) {
    var hue = ((x + offset) % 360);
    var c = hsvToRgb(hue, 1, 1);
    for (var y = 0; y < H; y++) {
      setPixel(x, y, c);
    }
  }
}

// Mode 2: Concentric rainbow circles
function rainbowCircles() {
  fill(rgb(0, 0, 0));
  var cx = W / 2;
  var cy = H / 2;
  var maxR = 100;
  var offset = frame * 3;
  
  for (var r = maxR; r > 0; r -= 3) {
    var hue = ((r * 3 + offset) % 360);
    var c = hsvToRgb(hue, 1, 1);
    fillCircle(cx, cy, r, c);
  }
}

// Mode 3: Plasma effect (simplified)
function plasma() {
  var t = frame * 0.1;
  for (var y = 0; y < H; y += 2) {
    for (var x = 0; x < W; x += 2) {
      var v1 = Math.sin(x * 0.03 + t);
      var v2 = Math.sin((y * 0.03 + t) * 0.5);
      var v3 = Math.sin((x * 0.02 + y * 0.02 + t) * 0.7);
      var v = (v1 + v2 + v3 + 3) / 6;  // Normalize to 0-1
      var hue = (v * 360 + frame) % 360;
      var c = hsvToRgb(hue, 1, 0.8);
      fillRect(x, y, 2, 2, c);
    }
  }
}

// Mode 4: Color wave
function colorWave() {
  var offset = frame * 0.05;
  for (var y = 0; y < H; y++) {
    var wave = Math.sin(y * 0.05 + offset) * 0.5 + 0.5;
    var hue = (y * 2 + frame * 2) % 360;
    var brightness = 0.5 + wave * 0.5;
    var c = hsvToRgb(hue, 1, brightness);
    hline(0, y, W, c);
  }
}

// Mode 5: Spiral rainbow
function spiral() {
  fill(rgb(0, 0, 0));
  var cx = W / 2;
  var cy = H / 2;
  var offset = frame * 0.02;
  
  for (var a = 0; a < 720; a += 3) {
    var rad = (a + offset * 180) * 3.14159 / 180;
    var r = a / 720 * 80;
    var x = cx + Math.floor(Math.cos(rad) * r);
    var y = cy + Math.floor(Math.sin(rad) * r * 0.85);
    if (x >= 0 && x < W && y >= 0 && y < H) {
      var hue = (a + frame * 3) % 360;
      var c = hsvToRgb(hue, 1, 1);
      fillRect(x - 1, y - 1, 3, 3, c);
    }
  }
}

var modes = [rainbowBars, rainbowCircles, plasma, colorWave, spiral];
var modeNames = ['Rainbow Bars', 'Rainbow Circles', 'Plasma', 'Color Wave', 'Spiral'];
var modeFrame = 0;
var modeDuration = 150;  // Frames per mode

function animate() {
  // Run current mode
  modes[mode]();
  flush();
  
  frame++;
  modeFrame++;
  
  // Switch mode every N frames
  if (modeFrame >= modeDuration) {
    modeFrame = 0;
    mode = (mode + 1) % modes.length;
    console.log('Mode: ' + modeNames[mode]);
  }
}

console.log('Starting rainbow animation...');
console.log('Mode: ' + modeNames[mode]);
var animInterval = setInterval(animate, 33);  // ~30fps

// Run for 30 seconds
setTimeout(function() {
  clearInterval(animInterval);
  console.log('Rainbow demo complete!');
}, 30000);
