// Button Input Example for mcujs
// Copy this file to your Pico as index.js
//
// Demonstrates reading button input with software debounce
// Connect a button between GPIO 15 and GND

const BUTTON_PIN = 15;  // Button connected to GPIO 15
const LED_PIN = 25;     // Onboard LED

// Initialize pins
GPIO.init(BUTTON_PIN, GPIO.INPUT_PULLUP);  // Use internal pull-up
GPIO.init(LED_PIN, GPIO.OUTPUT);

console.log("Button example starting...");
console.log("Button on GPIO", BUTTON_PIN, "(press to toggle LED)");
console.log("LED on GPIO", LED_PIN);

// State tracking
let ledState = false;
let lastButtonState = true;   // true = not pressed (pull-up)
let lastDebounceTime = 0;
const DEBOUNCE_DELAY = 50;    // 50ms debounce time

// Track button presses
let pressCount = 0;

// Poll button state
setInterval(() => {
    const currentTime = Date.now();
    const reading = GPIO.get(BUTTON_PIN);
    
    // If the button state changed, reset debounce timer
    if (reading !== lastButtonState) {
        lastDebounceTime = currentTime;
    }
    
    // If enough time has passed, accept the new state
    if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
        // Detect button press (transition from high to low with pull-up)
        if (!reading && lastButtonState) {
            // Button was just pressed
            pressCount++;
            ledState = !ledState;
            GPIO.set(LED_PIN, ledState);
            console.log("Button pressed! Count:", pressCount, "LED:", ledState ? "ON" : "OFF");
        }
    }
    
    lastButtonState = reading;
}, 10);  // Check every 10ms

console.log("Waiting for button presses...");
