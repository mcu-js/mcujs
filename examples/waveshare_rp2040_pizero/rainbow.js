/*
 * Rainbow Demo for Waveshare RP2040-PiZero
 * Animated rainbow color bars - simple and fast!
 * Run time: 30 seconds
 */

var dvi = {
  width: DVI.width, height: DVI.height, byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(p, l) { DVI.show(p, l); }
};
screen.init(dvi);
console.log('Rainbow Demo');

var w = screen.getWidth();
var h = screen.getHeight();

function hue2rgb(hue) {
  hue = hue & 255;
  var r, g, b;
  if (hue < 85) { r = hue * 3; g = 255 - hue * 3; b = 0; }
  else if (hue < 170) { hue -= 85; r = 255 - hue * 3; g = 0; b = hue * 3; }
  else { hue -= 170; r = 0; g = hue * 3; b = 255 - hue * 3; }
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

var offset = 0;
var count = 0;
var barHeight = 10;

var id = setInterval(function() {
  for (var y = 0; y < h; y += barHeight) {
    var hue = Math.floor((y + offset) * 2) & 255;
    screen.fillRect(0, y, w, barHeight, hue2rgb(hue));
  }
  screen.drawText(2, 2, 'F:' + count, screen.WHITE, 1);
  screen.show();
  offset += 4;
  count++;
  if (count >= 600) {
    clearInterval(id);
    screen.fill(0);
    screen.drawText(30, 50, 'Done!', screen.GREEN, 2);
    screen.show();
    console.log('Demo complete!');
  }
}, 50);

console.log('Running 30 seconds...');
