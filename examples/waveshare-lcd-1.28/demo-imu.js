// demo-imu.js - IMU visualization demo for Waveshare LCD 1.28
// Shows accelerometer data as a bubble level / spirit level

console.log('=== IMU Demo ===');

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

function hline(x,y,w,c) { fillRect(x,y,w,1,c); }
function vline(x,y,h,c) { fillRect(x,y,1,h,c); }

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

// IMU setup (QMI8658)
var IMU_ADDR = 0x6B;
var imuPins = { i2cBus: 1, sda: 6, scl: 7 };

function writeIMUReg(reg, value) {
  I2C.write(imuPins.i2cBus, IMU_ADDR, [reg, value]);
}

function readIMUReg(reg) {
  I2C.write(imuPins.i2cBus, IMU_ADDR, [reg]);
  return I2C.read(imuPins.i2cBus, IMU_ADDR, 1)[0];
}

function readIMURegs(reg, len) {
  I2C.write(imuPins.i2cBus, IMU_ADDR, [reg]);
  return I2C.read(imuPins.i2cBus, IMU_ADDR, len);
}

function toSigned16(low, high) {
  var val = (high << 8) | low;
  if (val >= 32768) val -= 65536;
  return val;
}

function initIMU() {
  // Initialize I2C
  I2C.init(imuPins.i2cBus, imuPins.sda, imuPins.scl, 400000);
  
  // Check WHO_AM_I
  var id = readIMUReg(0x00);
  console.log('IMU ID: 0x' + id.toString(16));
  
  if (id !== 0x05) {
    // Try alternate address
    IMU_ADDR = 0x6A;
    id = readIMUReg(0x00);
    console.log('Trying 0x6A, ID: 0x' + id.toString(16));
  }
  
  // Configure IMU
  writeIMUReg(0x02, 0x40);  // CTRL1: address auto-increment
  writeIMUReg(0x03, 0x15);  // CTRL2: ACC ±2g, 470Hz
  writeIMUReg(0x04, 0x45);  // CTRL3: GYR ±256dps, 470Hz
  writeIMUReg(0x06, 0x00);  // CTRL5: no LPF
  writeIMUReg(0x08, 0x03);  // CTRL7: enable ACC + GYR
  
  board.delay(50);
  console.log('IMU initialized');
}

function readAccel() {
  var data = readIMURegs(0x35, 6);
  var ax = toSigned16(data[0], data[1]) / 16384.0;  // ±2g scale
  var ay = toSigned16(data[2], data[3]) / 16384.0;
  var az = toSigned16(data[4], data[5]) / 16384.0;
  return { x: ax, y: ay, z: az };
}

// Colors
var BLACK = rgb(0,0,0);
var WHITE = rgb(255,255,255);
var RED = rgb(255,0,0);
var GREEN = rgb(0,255,0);
var BLUE = rgb(0,0,255);
var YELLOW = rgb(255,255,0);
var CYAN = rgb(0,255,255);
var GRAY = rgb(64,64,64);
var DARK_GREEN = rgb(0,64,0);

// Initialize
initScreen();
initIMU();

// Smoothed values
var smoothX = 0, smoothY = 0;
var alpha = 0.2;  // Smoothing factor

// Animation loop
function update() {
  var accel = readAccel();
  
  // Smooth the values
  smoothX = smoothX * (1 - alpha) + accel.x * alpha;
  smoothY = smoothY * (1 - alpha) + accel.y * alpha;
  
  // Clear screen with dark background
  fill(rgb(16, 24, 16));
  
  // Draw crosshairs
  var cx = 120, cy = 120;
  hline(20, cy, 200, GRAY);
  vline(cx, 20, 200, GRAY);
  
  // Draw concentric circles (level indicators)
  drawCircle(cx, cy, 30, GRAY);
  drawCircle(cx, cy, 60, GRAY);
  drawCircle(cx, cy, 90, GRAY);
  
  // Calculate bubble position based on accelerometer
  // Map accelerometer values to screen position
  // Accelerometer X/Y when flat should be ~0
  // Tilt moves the bubble opposite to gravity
  var bubbleX = cx - Math.floor(smoothX * 80);  // Invert for natural feel
  var bubbleY = cy + Math.floor(smoothY * 80);  // Y is inverted
  
  // Clamp to visible area
  bubbleX = Math.max(40, Math.min(200, bubbleX));
  bubbleY = Math.max(40, Math.min(200, bubbleY));
  
  // Calculate distance from center (for color)
  var dx = bubbleX - cx;
  var dy = bubbleY - cy;
  var dist = Math.sqrt(dx * dx + dy * dy);
  
  // Draw bubble
  var bubbleColor;
  if (dist < 15) {
    bubbleColor = GREEN;  // Level!
  } else if (dist < 40) {
    bubbleColor = YELLOW;  // Close
  } else {
    bubbleColor = RED;  // Tilted
  }
  
  // Draw bubble shadow
  fillCircle(bubbleX + 3, bubbleY + 3, 18, rgb(0, 32, 0));
  
  // Draw bubble
  fillCircle(bubbleX, bubbleY, 18, bubbleColor);
  
  // Draw highlight on bubble
  fillCircle(bubbleX - 5, bubbleY - 5, 6, WHITE);
  
  // Draw center target
  drawCircle(cx, cy, 8, dist < 15 ? GREEN : WHITE);
  drawCircle(cx, cy, 7, dist < 15 ? GREEN : WHITE);
  
  // Draw tilt indicators as bars
  var barWidth = 10;
  var barMaxH = 60;
  
  // X tilt bar (left side)
  var xBarH = Math.abs(smoothX) * barMaxH;
  var xBarY = smoothX > 0 ? cy - xBarH : cy;
  fillRect(15, cy - barMaxH, barWidth, barMaxH * 2, rgb(32, 32, 32));
  fillRect(15, xBarY, barWidth, xBarH, CYAN);
  
  // Y tilt bar (bottom)
  var yBarH = Math.abs(smoothY) * barMaxH;
  var yBarX = smoothY > 0 ? cx : cx - yBarH;
  fillRect(cx - barMaxH, 215, barMaxH * 2, barWidth, rgb(32, 32, 32));
  fillRect(yBarX, 215, yBarH, barWidth, CYAN);
  
  // Flush
  flush();
}

console.log('Tilt the board to move the bubble!');
console.log('Green = level, Yellow = close, Red = tilted');

// Run update loop
var imuInterval = setInterval(update, 33);  // ~30 FPS

// Run for 60 seconds
setTimeout(function() {
  clearInterval(imuInterval);
  console.log('Demo complete!');
}, 60000);
