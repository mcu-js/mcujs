// Simple LED Blink Demo for Pico 2
// The onboard LED is on GPIO 25 (Pico 2, not Pico 2 W)

const LED_PIN = 25;

GPIO.init(LED_PIN, GPIO.OUTPUT);

console.log("Blinking LED on GPIO", LED_PIN);

setInterval(() => {
    GPIO.toggle(LED_PIN);
}, 500);
