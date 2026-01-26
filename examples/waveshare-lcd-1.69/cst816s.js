// cst816s.js - CST816S Touch Controller Driver for mcujs
// I2C capacitive touch controller for Waveshare RP2350-Touch-LCD-1.69

// I2C address
var CST816S_ADDR = 0x15;

// Registers
var REG_GESTURE = 0x01;
var REG_FINGER_NUM = 0x02;
var REG_XPOS_H = 0x03;
var REG_XPOS_L = 0x04;
var REG_YPOS_H = 0x05;
var REG_YPOS_L = 0x06;
var REG_CHIP_ID = 0xA7;
var REG_PROJ_ID = 0xA8;
var REG_FW_VER = 0xA9;

// Gesture codes
var GESTURE_NONE = 0x00;
var GESTURE_SWIPE_UP = 0x01;
var GESTURE_SWIPE_DOWN = 0x02;
var GESTURE_SWIPE_LEFT = 0x03;
var GESTURE_SWIPE_RIGHT = 0x04;
var GESTURE_SINGLE_TAP = 0x05;
var GESTURE_DOUBLE_TAP = 0x0B;
var GESTURE_LONG_PRESS = 0x0C;

// Pin configuration (Waveshare RP2350-Touch-LCD-1.69)
var DEFAULT_PINS = {
  i2cBus: 1,
  sda: 6,
  scl: 7,
  intPin: 21,
  rstPin: 22
};

// Display dimensions for coordinate mapping
var DISPLAY_WIDTH = 240;
var DISPLAY_HEIGHT = 280;

// Create touch driver
// Options: { i2cBus, sda, scl, intPin, rstPin, baudrate, horizontal }
function createTouchDriver(options) {
  options = options || {};
  
  var i2cBus = options.i2cBus !== undefined ? options.i2cBus : DEFAULT_PINS.i2cBus;
  var sda = options.sda !== undefined ? options.sda : DEFAULT_PINS.sda;
  var scl = options.scl !== undefined ? options.scl : DEFAULT_PINS.scl;
  var intPin = options.intPin !== undefined ? options.intPin : DEFAULT_PINS.intPin;
  var rstPin = options.rstPin !== undefined ? options.rstPin : DEFAULT_PINS.rstPin;
  var baudrate = options.baudrate || 400000;  // 400 kHz
  var horizontal = options.horizontal !== undefined ? options.horizontal : false;
  
  var initialized = false;
  var width = horizontal ? DISPLAY_HEIGHT : DISPLAY_WIDTH;
  var height = horizontal ? DISPLAY_WIDTH : DISPLAY_HEIGHT;
  
  // Write register
  function writeReg(reg, value) {
    I2C.write(i2cBus, CST816S_ADDR, [reg, value]);
  }
  
  // Read register
  function readReg(reg) {
    I2C.write(i2cBus, CST816S_ADDR, [reg]);
    var data = I2C.read(i2cBus, CST816S_ADDR, 1);
    return data[0];
  }
  
  // Read multiple registers
  function readRegs(reg, len) {
    I2C.write(i2cBus, CST816S_ADDR, [reg]);
    return I2C.read(i2cBus, CST816S_ADDR, len);
  }
  
  // Hardware reset
  function reset() {
    GPIO.set(rstPin, 0);
    board.delay(10);
    GPIO.set(rstPin, 1);
    board.delay(50);
  }
  
  // Initialize touch controller
  function init() {
    if (initialized) return;
    
    // Initialize reset pin
    GPIO.init(rstPin, GPIO.OUTPUT);
    GPIO.set(rstPin, 1);
    
    // Initialize interrupt pin (optional, for detecting touch)
    GPIO.init(intPin, GPIO.INPUT);
    
    // Initialize I2C
    I2C.init(i2cBus, sda, scl, baudrate);
    
    // Reset the chip
    reset();
    
    initialized = true;
  }
  
  // Get chip ID (should be 0xB5 for CST816S)
  function getChipId() {
    return readReg(REG_CHIP_ID);
  }
  
  // Check if touch is detected
  function isTouched() {
    // INT pin goes low when touched (active low)
    return GPIO.get(intPin) === 0;
  }
  
  // Read touch data
  function read() {
    var data = readRegs(REG_GESTURE, 6);
    
    var gesture = data[0];
    var fingers = data[1];
    var rawX = ((data[2] & 0x0F) << 8) | data[3];
    var rawY = ((data[4] & 0x0F) << 8) | data[5];
    var event = (data[2] >> 6) & 0x03;  // 0=down, 1=up, 2=contact
    
    // Map coordinates based on orientation
    var x, y;
    if (horizontal) {
      // In horizontal mode, rotate coordinates
      x = rawY;
      y = DISPLAY_WIDTH - 1 - rawX;
    } else {
      x = rawX;
      y = rawY;
    }
    
    return {
      touched: fingers > 0,
      x: x,
      y: y,
      rawX: rawX,
      rawY: rawY,
      fingers: fingers,
      gesture: gesture,
      event: event
    };
  }
  
  // Get gesture name
  function getGestureName(gesture) {
    switch (gesture) {
      case GESTURE_SWIPE_UP: return 'swipe_up';
      case GESTURE_SWIPE_DOWN: return 'swipe_down';
      case GESTURE_SWIPE_LEFT: return 'swipe_left';
      case GESTURE_SWIPE_RIGHT: return 'swipe_right';
      case GESTURE_SINGLE_TAP: return 'tap';
      case GESTURE_DOUBLE_TAP: return 'double_tap';
      case GESTURE_LONG_PRESS: return 'long_press';
      default: return 'none';
    }
  }
  
  return {
    init: init,
    reset: reset,
    read: read,
    isTouched: isTouched,
    getChipId: getChipId,
    getGestureName: getGestureName,
    
    // Properties
    width: width,
    height: height,
    
    // Constants
    GESTURE_NONE: GESTURE_NONE,
    GESTURE_SWIPE_UP: GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN: GESTURE_SWIPE_DOWN,
    GESTURE_SWIPE_LEFT: GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT: GESTURE_SWIPE_RIGHT,
    GESTURE_SINGLE_TAP: GESTURE_SINGLE_TAP,
    GESTURE_DOUBLE_TAP: GESTURE_DOUBLE_TAP,
    GESTURE_LONG_PRESS: GESTURE_LONG_PRESS
  };
}

// Export
if (typeof module !== 'undefined') {
  module.exports = {
    createTouchDriver: createTouchDriver,
    CST816S_ADDR: CST816S_ADDR,
    DEFAULT_PINS: DEFAULT_PINS,
    DISPLAY_WIDTH: DISPLAY_WIDTH,
    DISPLAY_HEIGHT: DISPLAY_HEIGHT
  };
}
