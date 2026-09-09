'use strict';

// Firmware-packaged Canvas. Hardware and presentation stay inside display backends.
var native = require('mcujs:canvas-native');
var names = {
  black: '#000000', silver: '#c0c0c0', gray: '#808080', white: '#ffffff',
  maroon: '#800000', red: '#ff0000', purple: '#800080', fuchsia: '#ff00ff',
  green: '#008000', lime: '#00ff00', olive: '#808000', yellow: '#ffff00',
  navy: '#000080', blue: '#0000ff', teal: '#008080', aqua: '#00ffff',
  transparent: '#00000000'
};
function clamp(value, max) { return Math.max(0, Math.min(max, value)); }
function color(r, g, b, a) {
  var text;
  if (a === 1) {
    text = '#' + (0x1000000 + r * 65536 + g * 256 + b).toString(16).slice(1);
  } else {
    text = 'rgba(' + r + ', ' + g + ', ' + b + ', ' + Math.round(a * 1000) / 1000 + ')';
  }
  return { text: text, rgba: [r / 255, g / 255, b / 255, a] };
}
function parseColor(value) {
  if (typeof value === 'symbol') throw new TypeError('Cannot convert Symbol to a color');
  var text = String(value).trim().toLowerCase();
  if (Object.prototype.hasOwnProperty.call(names, text)) text = names[text];
  if (/^#(?:[0-9a-f]{3}|[0-9a-f]{4}|[0-9a-f]{6}|[0-9a-f]{8})$/.test(text)) {
    var hex = text.slice(1);
    if (hex.length <= 4) hex = hex.replace(/./g, function (c) { return c + c; });
    return color(parseInt(hex.slice(0, 2), 16), parseInt(hex.slice(2, 4), 16),
      parseInt(hex.slice(4, 6), 16), hex.length === 8 ? parseInt(hex.slice(6, 8), 16) / 255 : 1);
  }
  // Deliberately limited to legacy comma syntax, not a full CSS parser.
  var match = /^(rgb|rgba)\((.*)\)$/.exec(text);
  if (!match) return null;
  var parts = match[2].split(',');
  if (parts.length !== (match[1] === 'rgb' ? 3 : 4)) return null;
  var channels = [];
  var percent;
  for (var i = 0; i < parts.length; i++) {
    var part = parts[i].trim();
    if (!/^[+-]?(?:\d+(?:\.\d+)?|\.\d+)%?$/.test(part)) return null;
    var isPercent = part.charAt(part.length - 1) === '%';
    if (i === 0) percent = isPercent;
    if (i < 3 && isPercent !== percent) return null;
    var n = parseFloat(part);
    if (!isFinite(n)) return null;
    channels.push(i < 3 ? Math.round(clamp(isPercent ? n / 100 * 255 : n, 255)) :
      clamp(isPercent ? n / 100 : n, 1));
  }
  return color(channels[0], channels[1], channels[2], channels.length === 4 ? channels[3] : 1);
}
function createDisplay(backend) {
  var closed = false;
  var display = {};
  var canvas = {};
  var context = {
    getContextAttributes: function () { return { alpha: false }; }
  };
  var fillColor = color(0, 0, 0, 1);
  var strokeColor = color(0, 0, 0, 1);
  var lineWidth = 1;
  Object.defineProperties(context, {
    fillStyle: {
      enumerable: true,
      get: function () { return fillColor.text; },
      set: function (value) { var parsed = parseColor(value); if (parsed) fillColor = parsed; }
    },
    strokeStyle: {
      enumerable: true,
      get: function () { return strokeColor.text; },
      set: function (value) { var parsed = parseColor(value); if (parsed) strokeColor = parsed; }
    },
    lineWidth: {
      enumerable: true,
      get: function () { return lineWidth; },
      set: function (value) { var n = +value; if (isFinite(n) && n > 0) lineWidth = n; }
    }
  });
  Object.defineProperties(canvas, {
    width: { enumerable: true, value: backend.width },
    height: { enumerable: true, value: backend.height }
  });
  Object.defineProperty(context, 'canvas', { enumerable: true, value: canvas });
  canvas.getContext = function (type) {
    return String(type) === '2d' ? context : null;
  };
  var path = [];
  var hasSubpath = false;
  var canClose = false;
  // Limit counts numeric entries (including opcodes), not bytes or vertices.
  var MAX_PATH_ENTRIES = 128;

  function coordinates(args, count) {
    if (args.length < count) throw new TypeError('Not enough arguments');
    var values = [];
    var finite = true;
    for (var i = 0; i < count; i++) {
      // Unary + follows ToNumber, including rejecting Symbols and BigInts.
      var value = +args[i];
      values.push(value);
      if (!isFinite(value)) finite = false;
    }
    return finite ? values : null;
  }

  function append(command) {
    if (path.length + command.length > MAX_PATH_ENTRIES) {
      throw new RangeError('Canvas path exceeds 128 numeric entries; use beginPath()');
    }
    for (var i = 0; i < command.length; i++) path.push(command[i]);
  }

  context.beginPath = function () {
    path = [];
    hasSubpath = false;
    canClose = false;
  };
  context.moveTo = function (x, y) {
    var p = coordinates(arguments, 2);
    if (!p) return;
    append([1, p[0], p[1]]);
    hasSubpath = true;
    canClose = false;
  };
  context.lineTo = function (x, y) {
    var p = coordinates(arguments, 2);
    if (!p) return;
    append([hasSubpath ? 2 : 1, p[0], p[1]]);
    canClose = hasSubpath;
    hasSubpath = true;
  };
  context.closePath = function () {
    if (!canClose) return;
    append([3]);
    // Closing creates a new subpath at the old start, with just one point.
    canClose = false;
  };
  context.rect = function (x, y, width, height) {
    var p = coordinates(arguments, 4);
    if (!p) return;
    append([4, p[0], p[1], p[2], p[3]]);
    hasSubpath = true;
    canClose = false;
  };
  context.fill = function (fillRule) {
    ensureOpen();
    var rule = fillRule === undefined ? 'nonzero' : String(fillRule);
    if (rule === 'evenodd') throw new RangeError('Only nonzero fill is supported');
    if (rule !== 'nonzero') throw new TypeError('Invalid fill rule');
    if (path.length) backend.draw(path.slice(), 'fill', fillColor.rgba.slice(), lineWidth);
  };
  context.stroke = function () {
    ensureOpen();
    if (path.length) backend.draw(path.slice(), 'stroke', strokeColor.rgba.slice(), lineWidth);
  };
  function drawRect(args, mode, clear) {
    ensureOpen();
    var p = coordinates(args, 4);
    if (!p) return;
    if (mode === 'fill' ? p[2] === 0 || p[3] === 0 : p[2] === 0 && p[3] === 0) return;
    backend.draw([4, p[0], p[1], p[2], p[3]], mode,
      clear ? [0, 0, 0, 1] : (mode === 'fill' ? fillColor : strokeColor).rgba.slice(), lineWidth);
  }
  context.fillRect = function (x, y, width, height) { drawRect(arguments, 'fill'); };
  context.strokeRect = function (x, y, width, height) { drawRect(arguments, 'stroke'); };
  context.clearRect = function (x, y, width, height) { drawRect(arguments, 'fill', true); };

  var stack = [];
  context.save = function () {
    // Color records are immutable internally; do not reparse their rounded CSS strings.
    stack.push({ fill: fillColor, stroke: strokeColor, width: lineWidth });
  };
  context.restore = function () {
    if (!stack.length) return;
    var state = stack.pop();
    fillColor = state.fill;
    strokeColor = state.stroke;
    lineWidth = state.width;
  };

  Object.defineProperty(display, 'canvas', { enumerable: true, value: canvas });
  function ensureOpen() {
    if (closed) throw new Error('Display is closed');
  }
  display.close = function () {
    if (closed) return;
    backend.close();
    closed = true;
  };
  return display;
}

function connect(kind, options) {
  return createDisplay(native.open(kind, options === undefined ? {} : options));
}

var defaultDisplay;
function getDefaultDisplay() {
  if (!defaultDisplay) defaultDisplay = connect('default');
  return defaultDisplay;
}

module.exports = { connect: connect };
Object.defineProperties(module.exports, {
  canvas: { enumerable: true, get: function () { return getDefaultDisplay().canvas; } },
  display: { enumerable: true, get: getDefaultDisplay }
});
