// Application logic: no board names, GPIOs, controllers, or framebuffer pointers.
module.exports = function draw(display) {
  var canvas = display.canvas;
  var ctx = canvas.getContext('2d');
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = 'white';
  ctx.fillRect(0, 0, 8, 8);
};
