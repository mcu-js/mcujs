// LED Blink Example for mcujs
// Copy this file to your Pico as index.js
//
// This blinks the onboard LED on Pico (GPIO 25)

const LED_PIN = 25;  // Onboard LED on Pico/Pico 2

// Initialize LED pin as output
GPIO.init(LED_PIN, GPIO.OUTPUT);

console.log("Blink example starting...");
console.log("LED pin:", LED_PIN);

// Blink LED every 500ms
let ledState = false;

setInterval(() => {
    ledState = !ledState;
    GPIO.set(LED_PIN, ledState);
    console.log("LED:", ledState ? "ON" : "OFF");
}, 500);

console.log("Blinking LED on GPIO", LED_PIN);
