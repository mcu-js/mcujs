/*
 * Pong Game Demo for Waveshare RP2040-PiZero
 * 
 * Classic Pong game with AI opponent.
 * Demonstrates game loop, collision detection, and scoring.
 * 
 * Controls: None needed - both paddles are AI controlled
 * Run time: 60 seconds then exits cleanly
 */

// Initialize DVI display
var dvi = {
  width: DVI.width,
  height: DVI.height,
  byteOrder: 'native',
  init: function() { DVI.init(); DVI.start(); },
  show: function(ptr, len) { DVI.show(ptr, len); }
};

screen.init(dvi);
console.log('Pong Game Demo');

var w = screen.getWidth();
var h = screen.getHeight();

// Game constants
var PADDLE_WIDTH = 4;
var PADDLE_HEIGHT = 20;
var PADDLE_SPEED = 2;
var BALL_SIZE = 4;
var BALL_SPEED = 2;

// Game state
var ball = {
  x: w / 2,
  y: h / 2,
  vx: BALL_SPEED,
  vy: BALL_SPEED * 0.7
};

var leftPaddle = {
  x: 8,
  y: h / 2 - PADDLE_HEIGHT / 2
};

var rightPaddle = {
  x: w - 8 - PADDLE_WIDTH,
  y: h / 2 - PADDLE_HEIGHT / 2
};

var score = {
  left: 0,
  right: 0
};

var frameCount = 0;
var running = true;

// Reset ball to center
function resetBall(direction) {
  ball.x = w / 2;
  ball.y = h / 2;
  ball.vx = BALL_SPEED * direction;
  ball.vy = BALL_SPEED * (Math.random() > 0.5 ? 0.7 : -0.7);
}

// AI paddle control
function updatePaddle(paddle, targetY) {
  var paddleCenter = paddle.y + PADDLE_HEIGHT / 2;
  if (paddleCenter < targetY - 2) {
    paddle.y += PADDLE_SPEED;
  } else if (paddleCenter > targetY + 2) {
    paddle.y -= PADDLE_SPEED;
  }
  
  // Keep paddle on screen
  if (paddle.y < 0) paddle.y = 0;
  if (paddle.y > h - PADDLE_HEIGHT) paddle.y = h - PADDLE_HEIGHT;
}

// Check paddle collision
function checkPaddleCollision(paddle) {
  if (ball.x >= paddle.x && ball.x <= paddle.x + PADDLE_WIDTH &&
      ball.y >= paddle.y && ball.y <= paddle.y + PADDLE_HEIGHT) {
    return true;
  }
  return false;
}

// Game update
function update() {
  // Move ball
  ball.x += ball.vx;
  ball.y += ball.vy;
  
  // Ball collision with top/bottom
  if (ball.y <= 0 || ball.y >= h - BALL_SIZE) {
    ball.vy = -ball.vy;
    ball.y = ball.y <= 0 ? 0 : h - BALL_SIZE;
  }
  
  // Ball collision with paddles
  if (ball.vx < 0 && checkPaddleCollision(leftPaddle)) {
    ball.vx = -ball.vx;
    ball.x = leftPaddle.x + PADDLE_WIDTH;
    // Add some spin based on where ball hits paddle
    var hitPos = (ball.y - leftPaddle.y) / PADDLE_HEIGHT;
    ball.vy = (hitPos - 0.5) * BALL_SPEED * 2;
  }
  
  if (ball.vx > 0 && checkPaddleCollision(rightPaddle)) {
    ball.vx = -ball.vx;
    ball.x = rightPaddle.x - BALL_SIZE;
    var hitPos = (ball.y - rightPaddle.y) / PADDLE_HEIGHT;
    ball.vy = (hitPos - 0.5) * BALL_SPEED * 2;
  }
  
  // Scoring
  if (ball.x <= 0) {
    score.right++;
    resetBall(1);
  }
  if (ball.x >= w) {
    score.left++;
    resetBall(-1);
  }
  
  // AI paddle movement (track ball with some delay)
  updatePaddle(leftPaddle, ball.y);
  updatePaddle(rightPaddle, ball.y);
}

// Render game
function render() {
  // Clear screen
  screen.fill(screen.BLACK);
  
  // Draw center line
  for (var y = 0; y < h; y += 8) {
    screen.fillRect(w / 2 - 1, y, 2, 4, screen.GRAY);
  }
  
  // Draw paddles
  screen.fillRect(leftPaddle.x, leftPaddle.y, PADDLE_WIDTH, PADDLE_HEIGHT, screen.WHITE);
  screen.fillRect(rightPaddle.x, rightPaddle.y, PADDLE_WIDTH, PADDLE_HEIGHT, screen.WHITE);
  
  // Draw ball
  screen.fillRect(Math.floor(ball.x), Math.floor(ball.y), BALL_SIZE, BALL_SIZE, screen.WHITE);
  
  // Draw score
  screen.drawText(w / 4 - 6, 4, '' + score.left, screen.WHITE, 2);
  screen.drawText(3 * w / 4 - 6, 4, '' + score.right, screen.WHITE, 2);
  
  // Flush to display
  screen.show();
}

// Game loop at ~30 FPS
var intervalId = setInterval(function() {
  if (!running) return;
  
  update();
  render();
  
  frameCount++;
  
  // Stop after 60 seconds
  if (frameCount >= 1800) {
    running = false;
    clearInterval(intervalId);
    
    screen.fill(screen.BLACK);
    var winner = score.left > score.right ? 'LEFT' : (score.right > score.left ? 'RIGHT' : 'TIE');
    screen.drawText(30, 40, 'GAME OVER', screen.WHITE, 2);
    screen.drawText(20, 70, 'Winner: ' + winner, screen.GREEN, 1);
    screen.drawText(20, 85, score.left + ' - ' + score.right, screen.CYAN, 2);
    screen.show();
    
    console.log('Game over! Final score: ' + score.left + ' - ' + score.right);
  }
}, 33);

console.log('Pong running for 60 seconds...');
console.log('Both paddles are AI controlled');
