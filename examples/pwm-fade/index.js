// PWM LED Fade Example for mcujs
// Copy this file to your Pico as index.js
//
// This creates a smooth breathing effect on the onboard LED

const LED_PIN = 25;  // Onboard LED on Pico/Pico 2

// Initialize PWM on LED pin
// Frequency: 1000 Hz, Initial duty: 0%
PWM.init(LED_PIN, 1000, 0);

console.log("PWM Fade example starting...");

let brightness = 0;
let direction = 1;
const step = 1;
const maxBrightness = 100;
const updateInterval = 10;  // ms

setInterval(() => {
    brightness += direction * step;
    
    if (brightness >= maxBrightness) {
        brightness = maxBrightness;
        direction = -1;
    } else if (brightness <= 0) {
        brightness = 0;
        direction = 1;
    }
    
    PWM.setDuty(LED_PIN, brightness);
}, updateInterval);

console.log("LED breathing effect on GPIO", LED_PIN);
