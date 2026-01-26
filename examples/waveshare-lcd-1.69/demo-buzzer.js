// demo-buzzer.js - Buzzer Demo for Waveshare RP2350-Touch-LCD-1.69
// Plays all the melodies

var buzzer = require('./lib/waveshare-lcd-1.69/buzzer.js');

console.log('Buzzer Demo');
console.log('Playing all 6 melodies...');

buzzer.init();

console.log('Melody 1 - Bouncy tune');
buzzer.playMelody1();
board.delay(500);

console.log('Melody 2 - Nokia-style');
buzzer.playMelody2();
board.delay(500);

console.log('Melody 3 - Imperial March');
buzzer.playMelody3();
board.delay(500);

console.log('Melody 4 - Zelda Lullaby');
buzzer.playMelody4();
board.delay(500);

console.log('Melody 5 - Mario-style');
buzzer.playMelody5();
board.delay(500);

console.log('Melody 6 - Chemical Plant style');
buzzer.playMelody6();
board.delay(500);

console.log('Playing scale...');
buzzer.playScale();

console.log('Demo complete!');
