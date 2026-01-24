// demo-touch-draw.js - Touch drawing demo for Waveshare LCD 1.28
// Draw on the screen with your finger!

console.log('=== Touch Drawing Demo ===');

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

function fillCircle(cx,cy,r,c) {
  var x=r,y=0,e=0;
  while(x>=y) {
    hline(cx-x,cy+y,2*x+1,c);hline(cx-x,cy-y,2*x+1,c);
    hline(cx-y,cy+x,2*y+1,c);hline(cx-y,cy-x,2*y+1,c);
    y++;if(e<=0)e+=2*y+1;if(e>0){x--;e-=2*x+1;}
  }
}

// Touch controller (CST816S)
var TOUCH_ADDR = 0x15;
var touchPins = { i2cBus: 1, sda: 6, scl: 7, intPin: 21, rstPin: 22 };

function initTouch() {
  // Init reset pin
  GPIO.init(touchPins.rstPin, GPIO.OUTPUT);
  GPIO.set(touchPins.rstPin, 1);
  
  // Init interrupt pin
  GPIO.init(touchPins.intPin, GPIO.INPUT);
  
  // Init I2C
  I2C.init(touchPins.i2cBus, touchPins.sda, touchPins.scl, 400000);
  
  // Reset touch controller
  GPIO.set(touchPins.rstPin, 0);
  board.delay(10);
  GPIO.set(touchPins.rstPin, 1);
  board.delay(50);
  
  console.log('Touch initialized');
}

function readTouch() {
  try {
    // Write register address
    I2C.write(touchPins.i2cBus, TOUCH_ADDR, [0x01]);
    // Read 6 bytes
    var data = I2C.read(touchPins.i2cBus, TOUCH_ADDR, 6);
    
    var gesture = data[0];
    var fingers = data[1];
    var x = ((data[2] & 0x0F) << 8) | data[3];
    var y = ((data[4] & 0x0F) << 8) | data[5];
    
    return {
      touched: fingers > 0,
      x: x,
      y: y,
      fingers: fingers,
      gesture: gesture
    };
  } catch (e) {
    return { touched: false, x: 0, y: 0, fingers: 0, gesture: 0 };
  }
}

// Colors for drawing
var colors = [
  rgb(255, 0, 0),     // Red
  rgb(255, 128, 0),   // Orange
  rgb(255, 255, 0),   // Yellow
  rgb(0, 255, 0),     // Green
  rgb(0, 255, 255),   // Cyan
  rgb(0, 0, 255),     // Blue
  rgb(255, 0, 255),   // Magenta
  rgb(255, 255, 255)  // White
];
var colorIndex = 0;
var brushSize = 8;

// Initialize
initScreen();
initTouch();

// Draw initial UI
fill(rgb(32, 32, 32));

// Draw color palette at bottom
var paletteY = 210;
var paletteW = 25;
for (var i = 0; i < colors.length; i++) {
  fillRect(10 + i * (paletteW + 5), paletteY, paletteW, paletteW, colors[i]);
}

// Draw clear button
fillRect(200, 10, 30, 20, rgb(128, 0, 0));

flush();
console.log('Draw with your finger!');
console.log('Tap colors at bottom to change');
console.log('Tap top-right to clear');

// Drawing state
var lastX = -1, lastY = -1;
var needsFlush = false;

// Main loop
function update() {
  var touch = readTouch();
  
  if (touch.touched) {
    var x = touch.x;
    var y = touch.y;
    
    // Check if tapping color palette
    if (y >= paletteY && y < paletteY + paletteW) {
      var idx = Math.floor((x - 10) / (paletteW + 5));
      if (idx >= 0 && idx < colors.length) {
        colorIndex = idx;
        console.log('Color: ' + idx);
      }
    }
    // Check if tapping clear button
    else if (x >= 200 && y < 30) {
      fill(rgb(32, 32, 32));
      // Redraw palette
      for (var i = 0; i < colors.length; i++) {
        fillRect(10 + i * (paletteW + 5), paletteY, paletteW, paletteW, colors[i]);
      }
      fillRect(200, 10, 30, 20, rgb(128, 0, 0));
      console.log('Cleared');
      needsFlush = true;
    }
    // Draw on canvas
    else if (y > 35 && y < paletteY - 5) {
      fillCircle(x, y, brushSize, colors[colorIndex]);
      
      // Draw line between last and current position for smooth strokes
      if (lastX >= 0 && lastY >= 0) {
        var dx = x - lastX;
        var dy = y - lastY;
        var dist = Math.sqrt(dx * dx + dy * dy);
        if (dist > 2) {
          var steps = Math.floor(dist / 2);
          for (var s = 1; s < steps; s++) {
            var px = lastX + (dx * s / steps);
            var py = lastY + (dy * s / steps);
            fillCircle(Math.floor(px), Math.floor(py), brushSize, colors[colorIndex]);
          }
        }
      }
      
      lastX = x;
      lastY = y;
      needsFlush = true;
    }
  } else {
    lastX = -1;
    lastY = -1;
  }
  
  // Flush if needed
  if (needsFlush) {
    flush();
    needsFlush = false;
  }
}

// Run update loop
console.log('Starting touch loop...');
var touchInterval = setInterval(update, 16);  // ~60Hz polling

// Run for 60 seconds
setTimeout(function() {
  clearInterval(touchInterval);
  console.log('Demo complete!');
}, 60000);
