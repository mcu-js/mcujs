/*
 * Bouncing Balls Demo for Waveshare RP2040-PiZero
 * Multiple colored balls bouncing around the screen.
 * Run time: 30 seconds
 */

var dvi = {
  width: DVI.width, height: DVI.height, byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(p, l) { DVI.show(p, l); }
};
screen.init(dvi);
console.log('Bouncing Balls Demo');

var w = screen.getWidth();
var h = screen.getHeight();

// Ball data: x, y, vx, vy, radius, color
var balls = [
  {x: 40, y: 30, vx: 2, vy: 1.5, r: 8, c: screen.RED},
  {x: 80, y: 60, vx: -1.5, vy: 2, r: 10, c: screen.GREEN},
  {x: 120, y: 40, vx: 1.8, vy: -1.2, r: 6, c: screen.BLUE},
  {x: 60, y: 80, vx: -2, vy: -1.5, r: 9, c: screen.YELLOW},
  {x: 100, y: 50, vx: 1.5, vy: 1.8, r: 7, c: screen.CYAN}
];

var count = 0;

var id = setInterval(function() {
  screen.fill(0);
  
  for (var i = 0; i < balls.length; i++) {
    var b = balls[i];
    b.x += b.vx;
    b.y += b.vy;
    if (b.x < b.r || b.x > w - b.r) b.vx = -b.vx;
    if (b.y < b.r || b.y > h - b.r) b.vy = -b.vy;
    screen.fillCircle(Math.floor(b.x), Math.floor(b.y), b.r, b.c);
  }
  
  screen.drawText(2, 2, 'F:' + count, screen.WHITE, 1);
  screen.show();
  count++;
  
  if (count >= 900) {
    clearInterval(id);
    screen.fill(0);
    screen.drawText(30, 50, 'Done!', screen.GREEN, 2);
    screen.show();
    console.log('Demo complete!');
  }
}, 33);

console.log('Running 30 seconds...');
