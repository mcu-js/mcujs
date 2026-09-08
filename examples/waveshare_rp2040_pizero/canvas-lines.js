/* The drawing function is unchanged between a browser Canvas and MCU.js. */
function drawCanvasDemo(canvas) {
  var ctx = canvas.getContext('2d');
  ctx.fillStyle = '#08182e';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.strokeStyle = '#00ffff';
  ctx.lineWidth = 2;
  ctx.strokeRect(5, 5, 150, 110);
  ctx.beginPath();
  ctx.moveTo(16, 94);
  ctx.lineTo(144, 26);
  ctx.strokeStyle = '#ffffff';
  ctx.lineWidth = 3;
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(16, 26);
  ctx.lineTo(144, 94);
  ctx.strokeStyle = '#00ff80';
  ctx.lineWidth = 2;
  ctx.stroke();
  ctx.fillStyle = '#ff4060';
  ctx.fillRect(14, 14, 12, 12);
  ctx.fillStyle = '#ffd040';
  ctx.fillRect(134, 94, 12, 12);
}
if (typeof module !== 'undefined') module.exports = drawCanvasDemo;
