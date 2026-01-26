# Waveshare RP2040-PiZero DVI Demos

Example programs demonstrating DVI/HDMI output on the Waveshare RP2040-PiZero board.

## Requirements

- Waveshare RP2040-PiZero board
- HDMI monitor/TV connected
- mcujs firmware flashed

## Running the Demos

1. Copy the `.js` files to the device filesystem (via USB mass storage)
2. Connect to the REPL via serial
3. Run with `.run filename.js`

```
.run bouncing-balls.js
.run digital-clock.js
.run rainbow.js
.run pong.js
.run slideshow.js
```

## Demos

### bouncing-balls.js

Multiple colored balls bouncing around the screen. Demonstrates:
- Object-oriented JavaScript (Ball class with prototype methods)
- Multiple animated objects
- Collision detection with screen edges
- ~30 FPS animation

**Duration:** 30 seconds

### digital-clock.js

Digital clock display with animated seconds progress bar. Demonstrates:
- Text rendering with different sizes
- Progress bar graphics
- Periodic updates (1 second interval)
- Color customization with `screen.rgb()`

**Duration:** 60 seconds

### rainbow.js

Animated rainbow color bars scrolling across the screen. Demonstrates:
- HSV to RGB565 color conversion
- Smooth color gradients
- Fast rectangle-based rendering
- Simple animation loop

**Duration:** 30 seconds

### pong.js

Classic Pong game with AI-controlled paddles. Demonstrates:
- Game loop architecture
- Collision detection (ball vs paddles, ball vs walls)
- Simple AI (paddle tracks ball position)
- Score display
- Game state management

**Duration:** 60 seconds

### slideshow.js

Auto-advancing slideshow showcasing graphics capabilities. Demonstrates:
- Multiple slides with different content
- Shapes: circles, rectangles
- Color bars and checkerboard patterns
- Text rendering at different sizes
- Timer-based slide transitions

**Duration:** 30 seconds (8 slides, 3 seconds each)

## Screen API Reference

```javascript
// Initialize with DVI driver
var dvi = {
  width: DVI.width,
  height: DVI.height,
  byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(ptr, len) { DVI.show(ptr, len); }
};
screen.init(dvi);

// Drawing functions
screen.fill(color)                           // Fill entire screen
screen.setPixel(x, y, color)                 // Set single pixel
screen.fillRect(x, y, width, height, color)  // Filled rectangle
screen.fillCircle(cx, cy, radius, color)     // Filled circle
screen.drawLine(x0, y0, x1, y1, color)       // Line
screen.drawText(x, y, text, color, size)     // Text (size 1-3)
screen.show()                                // Flush to display

// Colors (RGB565)
screen.BLACK, screen.WHITE, screen.RED, screen.GREEN, screen.BLUE
screen.CYAN, screen.MAGENTA, screen.YELLOW, screen.ORANGE, screen.GRAY
screen.rgb(r, g, b)  // Custom color (0-255 each)

// Display info
screen.getWidth()   // 160
screen.getHeight()  // 120
```

## Display Specifications

| Property | Value |
|----------|-------|
| Output Resolution | 640x480 @ 60Hz |
| Framebuffer Size | 160x120 (4x scaled) |
| Color Depth | RGB565 (16-bit, 65536 colors) |
| Refresh Rate | 60 Hz |

## Tips

- Use `setInterval()` for animation loops, never `while(true)`
- Call `screen.show()` after drawing to flush the framebuffer
- Keep animations under 30 FPS for smooth performance
- Per-pixel operations are slow; prefer `fillRect()` and `fillCircle()`
- Demos auto-exit after their duration to return to REPL
