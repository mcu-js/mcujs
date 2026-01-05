// I2C Bus Scanner Example for mcujs
// Copy this file to your Pico as index.js
//
// Scans the I2C bus for connected devices and displays their addresses
// Default pins: SDA=GPIO4, SCL=GPIO5 (I2C0)

const I2C_BUS = 0;       // I2C bus 0
const SDA_PIN = 4;       // Default I2C0 SDA pin
const SCL_PIN = 5;       // Default I2C0 SCL pin
const BAUDRATE = 100000; // 100kHz (standard mode)

console.log("I2C Scanner Example");
console.log("===================");
console.log("Bus:", I2C_BUS);
console.log("SDA: GPIO", SDA_PIN);
console.log("SCL: GPIO", SCL_PIN);
console.log("Baudrate:", BAUDRATE, "Hz");
console.log("");

// Initialize I2C
I2C.init(I2C_BUS, SDA_PIN, SCL_PIN, BAUDRATE);

console.log("Scanning I2C bus for devices...");
console.log("");

// Scan all valid I2C addresses (0x08-0x77)
// Addresses 0x00-0x07 and 0x78-0x7F are reserved
const devices = [];

for (let addr = 0x08; addr <= 0x77; addr++) {
    try {
        // Try to read 1 byte from the address
        // If a device is present, this will succeed
        const result = I2C.read(I2C_BUS, addr, 1);
        if (result && result.length > 0) {
            devices.push(addr);
            console.log("Found device at 0x" + addr.toString(16).toUpperCase());
        }
    } catch (e) {
        // No device at this address - this is expected
    }
}

console.log("");
console.log("Scan complete!");
console.log("Found", devices.length, "device(s)");

if (devices.length > 0) {
    console.log("");
    console.log("Device addresses:");
    devices.forEach((addr) => {
        // Print common device identifications
        let deviceName = "Unknown";
        switch (addr) {
            case 0x27: case 0x3F:
                deviceName = "LCD (PCF8574)";
                break;
            case 0x3C: case 0x3D:
                deviceName = "OLED Display (SSD1306)";
                break;
            case 0x48: case 0x49: case 0x4A: case 0x4B:
                deviceName = "ADC (ADS1115/PCF8591)";
                break;
            case 0x50: case 0x51: case 0x52: case 0x53:
            case 0x54: case 0x55: case 0x56: case 0x57:
                deviceName = "EEPROM (24Cxx)";
                break;
            case 0x68:
                deviceName = "RTC (DS3231) or MPU6050";
                break;
            case 0x76: case 0x77:
                deviceName = "BME280/BMP280 Sensor";
                break;
        }
        console.log("  0x" + addr.toString(16).toUpperCase() + " - " + deviceName);
    });
}

console.log("");
console.log("Tip: Connect I2C devices to:");
console.log("  SDA -> GPIO", SDA_PIN);
console.log("  SCL -> GPIO", SCL_PIN);
console.log("  VCC -> 3.3V");
console.log("  GND -> GND");
