// qmi8658.js - QMI8658 IMU Driver for mcujs
// 6-axis accelerometer/gyroscope used on Waveshare RP2040 Touch LCD 1.28

// I2C address (depends on SA0 pin, typically 0x6A or 0x6B)
var QMI8658_ADDR = 0x6B;

// Registers
var REG_WHO_AM_I = 0x00;        // Should return 0x05
var REG_REVISION = 0x01;
var REG_CTRL1 = 0x02;           // Serial interface and sensor enable
var REG_CTRL2 = 0x03;           // Accelerometer settings
var REG_CTRL3 = 0x04;           // Gyroscope settings
var REG_CTRL5 = 0x06;           // Low pass filter
var REG_CTRL7 = 0x08;           // Enable sensors
var REG_CTRL9 = 0x0A;           // Host commands

var REG_TEMP_L = 0x33;          // Temperature low
var REG_TEMP_H = 0x34;          // Temperature high
var REG_AX_L = 0x35;            // Accel X low
var REG_AX_H = 0x36;            // Accel X high
var REG_AY_L = 0x37;            // Accel Y low
var REG_AY_H = 0x38;            // Accel Y high
var REG_AZ_L = 0x39;            // Accel Z low
var REG_AZ_H = 0x3A;            // Accel Z high
var REG_GX_L = 0x3B;            // Gyro X low
var REG_GX_H = 0x3C;            // Gyro X high
var REG_GY_L = 0x3D;            // Gyro Y low
var REG_GY_H = 0x3E;            // Gyro Y high
var REG_GZ_L = 0x3F;            // Gyro Z low
var REG_GZ_H = 0x40;            // Gyro Z high

// Accelerometer scale (±2g, ±4g, ±8g, ±16g)
var ACC_SCALE_2G = 0x00;
var ACC_SCALE_4G = 0x01;
var ACC_SCALE_8G = 0x02;
var ACC_SCALE_16G = 0x03;

// Gyroscope scale (±16, ±32, ±64, ±128, ±256, ±512, ±1024, ±2048 dps)
var GYR_SCALE_16DPS = 0x00;
var GYR_SCALE_32DPS = 0x01;
var GYR_SCALE_64DPS = 0x02;
var GYR_SCALE_128DPS = 0x03;
var GYR_SCALE_256DPS = 0x04;
var GYR_SCALE_512DPS = 0x05;
var GYR_SCALE_1024DPS = 0x06;
var GYR_SCALE_2048DPS = 0x07;

// Pin configuration
var DEFAULT_PINS = {
  i2cBus: 1,
  sda: 6,
  scl: 7,
  int1Pin: 23,
  int2Pin: 24
};

// Create IMU driver
function createIMUDriver(options) {
  options = options || {};
  
  var i2cBus = options.i2cBus !== undefined ? options.i2cBus : DEFAULT_PINS.i2cBus;
  var sda = options.sda !== undefined ? options.sda : DEFAULT_PINS.sda;
  var scl = options.scl !== undefined ? options.scl : DEFAULT_PINS.scl;
  var baudrate = options.baudrate || 400000;
  
  var initialized = false;
  var accScale = 2.0;   // g per LSB divisor
  var gyrScale = 16.0;  // dps per LSB divisor
  
  // Write register
  function writeReg(reg, value) {
    I2C.write(i2cBus, QMI8658_ADDR, [reg, value]);
  }
  
  // Read register
  function readReg(reg) {
    I2C.write(i2cBus, QMI8658_ADDR, [reg]);
    var data = I2C.read(i2cBus, QMI8658_ADDR, 1);
    return data[0];
  }
  
  // Read multiple registers
  function readRegs(reg, len) {
    I2C.write(i2cBus, QMI8658_ADDR, [reg]);
    return I2C.read(i2cBus, QMI8658_ADDR, len);
  }
  
  // Convert 16-bit signed value
  function toSigned16(low, high) {
    var val = (high << 8) | low;
    if (val >= 32768) val -= 65536;
    return val;
  }
  
  // Initialize IMU
  function init() {
    if (initialized) return;
    
    // Initialize I2C (may already be initialized by touch)
    I2C.init(i2cBus, sda, scl, baudrate);
    
    // Check WHO_AM_I
    var id = readReg(REG_WHO_AM_I);
    if (id !== 0x05) {
      console.log('QMI8658 not found, got ID: 0x' + id.toString(16));
      // Try alternate address
      QMI8658_ADDR = 0x6A;
      id = readReg(REG_WHO_AM_I);
      if (id !== 0x05) {
        console.log('QMI8658 not found at 0x6A either');
        return false;
      }
    }
    console.log('QMI8658 found, ID: 0x' + id.toString(16));
    
    // Configure CTRL1: Address auto-increment
    writeReg(REG_CTRL1, 0x40);
    
    // Configure CTRL2: Accelerometer ±2g, 470.7Hz ODR
    writeReg(REG_CTRL2, 0x15);  // ACC_FS=0 (±2g), ACC_ODR=5 (470.7Hz)
    accScale = 2.0;
    
    // Configure CTRL3: Gyroscope ±256dps, 470.7Hz ODR
    writeReg(REG_CTRL3, 0x45);  // GYR_FS=4 (±256dps), GYR_ODR=5 (470.7Hz)
    gyrScale = 256.0;
    
    // Configure CTRL5: Low pass filter
    writeReg(REG_CTRL5, 0x00);
    
    // Configure CTRL7: Enable accelerometer and gyroscope
    writeReg(REG_CTRL7, 0x03);  // Enable ACC and GYR
    
    board.delay(50);  // Wait for startup
    
    initialized = true;
    return true;
  }
  
  // Get WHO_AM_I value
  function getChipId() {
    return readReg(REG_WHO_AM_I);
  }
  
  // Read temperature
  function readTemperature() {
    var data = readRegs(REG_TEMP_L, 2);
    var raw = toSigned16(data[0], data[1]);
    return raw / 256.0;  // Convert to Celsius
  }
  
  // Read accelerometer (returns g values)
  function readAccel() {
    var data = readRegs(REG_AX_L, 6);
    var ax = toSigned16(data[0], data[1]);
    var ay = toSigned16(data[2], data[3]);
    var az = toSigned16(data[4], data[5]);
    
    // Convert to g (16-bit range maps to ±scale)
    var scale = accScale / 32768.0;
    return {
      x: ax * scale,
      y: ay * scale,
      z: az * scale
    };
  }
  
  // Read gyroscope (returns degrees per second)
  function readGyro() {
    var data = readRegs(REG_GX_L, 6);
    var gx = toSigned16(data[0], data[1]);
    var gy = toSigned16(data[2], data[3]);
    var gz = toSigned16(data[4], data[5]);
    
    // Convert to dps (16-bit range maps to ±scale)
    var scale = gyrScale / 32768.0;
    return {
      x: gx * scale,
      y: gy * scale,
      z: gz * scale
    };
  }
  
  // Read all sensor data
  function read() {
    var data = readRegs(REG_TEMP_L, 14);
    
    var temp = toSigned16(data[0], data[1]) / 256.0;
    
    var accScaleFactor = accScale / 32768.0;
    var ax = toSigned16(data[2], data[3]) * accScaleFactor;
    var ay = toSigned16(data[4], data[5]) * accScaleFactor;
    var az = toSigned16(data[6], data[7]) * accScaleFactor;
    
    var gyrScaleFactor = gyrScale / 32768.0;
    var gx = toSigned16(data[8], data[9]) * gyrScaleFactor;
    var gy = toSigned16(data[10], data[11]) * gyrScaleFactor;
    var gz = toSigned16(data[12], data[13]) * gyrScaleFactor;
    
    return {
      temperature: temp,
      accel: { x: ax, y: ay, z: az },
      gyro: { x: gx, y: gy, z: gz }
    };
  }
  
  return {
    init: init,
    read: read,
    readAccel: readAccel,
    readGyro: readGyro,
    readTemperature: readTemperature,
    getChipId: getChipId
  };
}

// Export
if (typeof module !== 'undefined') {
  module.exports = {
    createIMUDriver: createIMUDriver,
    QMI8658_ADDR: QMI8658_ADDR,
    DEFAULT_PINS: DEFAULT_PINS
  };
}
