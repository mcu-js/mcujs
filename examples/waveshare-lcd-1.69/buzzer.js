// buzzer.js - Buzzer Driver for mcujs
// PWM-based buzzer for Waveshare RP2350-Touch-LCD-1.69
// Buzzer is on GPIO 2

var BUZZER_PIN = 2;

// Note frequencies (Hz)
var NOTE = {
  REST: 0,
  C4: 262, CS4: 277, D4: 294, DS4: 311, E4: 330, F4: 349,
  FS4: 370, G4: 392, GS4: 415, A4: 440, AS4: 466, B4: 494,
  C5: 523, CS5: 554, D5: 587, DS5: 622, E5: 659, F5: 698,
  FS5: 740, G5: 784, GS5: 831, A5: 880, AS5: 932, B5: 988,
  C6: 1047, CS6: 1109, D6: 1175, DS6: 1245, E6: 1319, F6: 1397,
  FS6: 1480, G6: 1568, GS6: 1661, A6: 1760, AS6: 1865, B6: 1976
};

// Melody 1 - Bouncy tune
var MELODY_1 = [
  [NOTE.B4, 120], [NOTE.E5, 120], [NOTE.GS5, 120], [NOTE.B5, 120],
  [NOTE.A5, 240], [NOTE.GS5, 120], [NOTE.E5, 120],
  [NOTE.FS5, 360], [NOTE.REST, 120],
  [NOTE.B4, 120], [NOTE.E5, 120], [NOTE.GS5, 120], [NOTE.B5, 120],
  [NOTE.A5, 120], [NOTE.GS5, 120], [NOTE.FS5, 120], [NOTE.E5, 120],
  [NOTE.GS5, 360], [NOTE.REST, 120],
  [NOTE.E5, 120], [NOTE.FS5, 120], [NOTE.GS5, 120], [NOTE.A5, 120],
  [NOTE.B5, 240], [NOTE.A5, 120], [NOTE.GS5, 120],
  [NOTE.FS5, 120], [NOTE.E5, 120], [NOTE.FS5, 120], [NOTE.GS5, 120],
  [NOTE.A5, 360], [NOTE.REST, 240]
];

// Melody 2 - Classic Nokia-style tune
var MELODY_2 = [
  [NOTE.E5, 150], [NOTE.D5, 150], [NOTE.FS4, 300], [NOTE.GS4, 300],
  [NOTE.CS5, 150], [NOTE.B4, 150], [NOTE.D4, 300], [NOTE.E4, 300],
  [NOTE.B4, 150], [NOTE.A4, 150], [NOTE.CS4, 300], [NOTE.E4, 300],
  [NOTE.A4, 600], [NOTE.REST, 300]
];

// Melody 3 - March style (dramatic)
var MELODY_3 = [
  [NOTE.G4, 500], [NOTE.G4, 500], [NOTE.G4, 500], [NOTE.DS4, 350], [NOTE.AS4, 150],
  [NOTE.G4, 500], [NOTE.DS4, 350], [NOTE.AS4, 150], [NOTE.G4, 1000], [NOTE.REST, 200],
  [NOTE.D5, 500], [NOTE.D5, 500], [NOTE.D5, 500], [NOTE.DS5, 350], [NOTE.AS4, 150],
  [NOTE.FS4, 500], [NOTE.DS4, 350], [NOTE.AS4, 150], [NOTE.G4, 1000], [NOTE.REST, 200]
];

// Melody 4 - Zelda's Lullaby
var MELODY_4 = [
  [NOTE.B4, 600], [NOTE.D5, 300], [NOTE.A4, 900],
  [NOTE.B4, 600], [NOTE.D5, 300], [NOTE.A4, 900],
  [NOTE.B4, 300], [NOTE.D5, 300], [NOTE.A5, 300], [NOTE.G5, 600],
  [NOTE.D5, 300], [NOTE.C5, 300], [NOTE.B4, 300], [NOTE.A4, 900],
  [NOTE.REST, 300]
];

// Melody 5 - Platformer style (bouncy and fun)
var MELODY_5 = [
  [NOTE.E5, 150], [NOTE.E5, 150], [NOTE.REST, 150], [NOTE.E5, 150], [NOTE.REST, 150],
  [NOTE.C5, 150], [NOTE.E5, 300], [NOTE.G5, 300], [NOTE.REST, 300], [NOTE.G4, 300],
  [NOTE.REST, 300],
  [NOTE.C5, 300], [NOTE.REST, 150], [NOTE.G4, 300], [NOTE.REST, 150], [NOTE.E4, 300],
  [NOTE.REST, 150], [NOTE.A4, 300], [NOTE.B4, 300], [NOTE.AS4, 150], [NOTE.A4, 300],
  [NOTE.G4, 200], [NOTE.E5, 200], [NOTE.G5, 200], [NOTE.A5, 300], [NOTE.F5, 150],
  [NOTE.G5, 150], [NOTE.REST, 150], [NOTE.E5, 300], [NOTE.C5, 150], [NOTE.D5, 150],
  [NOTE.B4, 300]
];

// Melody 6 - Fast synth style (urgent, ascending runs)
var MELODY_6 = [
  // Fast ascending bass line
  [NOTE.E4, 100], [NOTE.G4, 100], [NOTE.B4, 100], [NOTE.E5, 100],
  [NOTE.D5, 100], [NOTE.B4, 100], [NOTE.G4, 100], [NOTE.E4, 100],
  [NOTE.FS4, 100], [NOTE.A4, 100], [NOTE.CS5, 100], [NOTE.FS5, 100],
  [NOTE.E5, 100], [NOTE.CS5, 100], [NOTE.A4, 100], [NOTE.FS4, 100],
  
  // Main hook
  [NOTE.G5, 150], [NOTE.FS5, 150], [NOTE.E5, 150], [NOTE.D5, 150],
  [NOTE.E5, 300], [NOTE.REST, 100],
  [NOTE.G5, 150], [NOTE.FS5, 150], [NOTE.E5, 150], [NOTE.D5, 150],
  [NOTE.CS5, 300], [NOTE.REST, 100],
  
  // Ascending urgency
  [NOTE.E4, 75], [NOTE.FS4, 75], [NOTE.G4, 75], [NOTE.A4, 75],
  [NOTE.B4, 75], [NOTE.CS5, 75], [NOTE.D5, 75], [NOTE.E5, 75],
  [NOTE.FS5, 75], [NOTE.G5, 75], [NOTE.A5, 75], [NOTE.B5, 200],
  [NOTE.REST, 100],
  
  // Ending phrase
  [NOTE.B5, 100], [NOTE.A5, 100], [NOTE.G5, 100], [NOTE.FS5, 100],
  [NOTE.E5, 100], [NOTE.D5, 100], [NOTE.CS5, 100], [NOTE.B4, 100],
  [NOTE.E5, 400]
];

var initialized = false;
var playing = false;

// Initialize buzzer
function init() {
  if (initialized) return;
  PWM.init(BUZZER_PIN, 440);
  PWM.setDuty(BUZZER_PIN, 0);
  initialized = true;
}

// Play a tone at given frequency
function tone(freq, duration) {
  if (!initialized) init();
  
  if (freq === 0 || freq === NOTE.REST) {
    PWM.setDuty(BUZZER_PIN, 0);
  } else {
    PWM.init(BUZZER_PIN, freq);
    PWM.setDuty(BUZZER_PIN, 0.5);
  }
  
  if (duration) {
    board.delay(duration);
    PWM.setDuty(BUZZER_PIN, 0);
  }
}

// Stop any sound
function stop() {
  PWM.setDuty(BUZZER_PIN, 0);
  playing = false;
}

// Play a beep
function beep(freq, duration) {
  freq = freq || 1000;
  duration = duration || 100;
  tone(freq, duration);
}

// Play a melody (array of [freq, duration] pairs)
function playMelody(melody, tempo) {
  if (!initialized) init();
  tempo = tempo || 1.0;
  playing = true;
  
  for (var i = 0; i < melody.length; i++) {
    if (!playing) break;
    var note = melody[i];
    var freq = note[0];
    var duration = Math.floor(note[1] / tempo);
    
    if (freq === 0 || freq === NOTE.REST) {
      PWM.setDuty(BUZZER_PIN, 0);
    } else {
      PWM.init(BUZZER_PIN, freq);
      PWM.setDuty(BUZZER_PIN, 0.5);
    }
    
    board.delay(Math.floor(duration * 0.9));
    PWM.setDuty(BUZZER_PIN, 0);
    board.delay(Math.floor(duration * 0.1));
  }
  
  stop();
}

// Play melody by number (1-6)
function play(num, tempo) {
  var melodies = [MELODY_1, MELODY_2, MELODY_3, MELODY_4, MELODY_5, MELODY_6];
  var idx = (num || 1) - 1;
  if (idx >= 0 && idx < melodies.length) {
    playMelody(melodies[idx], tempo || 1.0);
  }
}

// Named melody functions
function playMelody1(tempo) { playMelody(MELODY_1, tempo || 1.0); }
function playMelody2(tempo) { playMelody(MELODY_2, tempo || 1.0); }  // Nokia-style
function playMelody3(tempo) { playMelody(MELODY_3, tempo || 1.0); }  // Imperial March
function playMelody4(tempo) { playMelody(MELODY_4, tempo || 1.0); }  // Zelda Lullaby
function playMelody5(tempo) { playMelody(MELODY_5, tempo || 1.0); }  // Mario-style
function playMelody6(tempo) { playMelody(MELODY_6, tempo || 1.0); }  // Chemical Plant style

// Play a scale
function playScale() {
  var scale = [NOTE.C4, NOTE.D4, NOTE.E4, NOTE.F4, NOTE.G4, NOTE.A4, NOTE.B4, NOTE.C5];
  for (var i = 0; i < scale.length; i++) {
    tone(scale[i], 200);
    board.delay(50);
  }
}

// Export
if (typeof module !== 'undefined') {
  module.exports = {
    init: init,
    tone: tone,
    beep: beep,
    stop: stop,
    play: play,
    playMelody: playMelody,
    playMelody1: playMelody1,
    playMelody2: playMelody2,
    playMelody3: playMelody3,
    playMelody4: playMelody4,
    playMelody5: playMelody5,
    playMelody6: playMelody6,
    playScale: playScale,
    NOTE: NOTE,
    MELODY_1: MELODY_1,
    MELODY_2: MELODY_2,
    MELODY_3: MELODY_3,
    MELODY_4: MELODY_4,
    MELODY_5: MELODY_5,
    MELODY_6: MELODY_6,
    BUZZER_PIN: BUZZER_PIN
  };
}
