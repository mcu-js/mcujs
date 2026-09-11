// Header-only metadata for files on any fs mount (for example /app or /sd).
// This does not establish decodability or inspect compressed data/pixels.
var fs = require('fs');

function info(path) {
  var fd = fs.openSync(path, 'r');
  try {
    var size = fs.statSync(path).size;
    if (typeof size !== 'number' || !isFinite(size) || size < 2 || Math.floor(size) !== size) {
      throw new Error('Invalid image file size');
    }
    var bytes = new Uint8Array(128);
    var calls = 0;
    var limit = size;

    function check(position, length) {
      if (position < 0 || length < 0 || position > limit || length > limit - position) {
        throw new Error('Truncated image header or scan limit exceeded');
      }
    }
    function read(position, length) {
      check(position, length);
      if (length > bytes.length) throw new Error('Image header too large');
      var done = 0;
      while (done < length) {
        if (calls >= 128) throw new Error('Image header read budget exceeded');
        calls++;
        var n = fs.readSync(fd, bytes, done, length - done, position + done);
        if (typeof n !== 'number' || n <= 0 || n > length - done || Math.floor(n) !== n) {
          throw new Error('Truncated image header');
        }
        done += n;
      }
    }
    function be16(offset) { return bytes[offset] * 256 + bytes[offset + 1]; }
    function le16(offset) { return bytes[offset] + bytes[offset + 1] * 256; }
    function le32(offset) { return le16(offset) + le16(offset + 2) * 65536; }

    read(0, 2);
    if (bytes[0] === 66 && bytes[1] === 77) {
      read(0, 18);
      var pixelOffset = le32(10);
      var dib = le32(14);
      if (dib !== 40 && dib !== 52 && dib !== 56 && dib !== 108 && dib !== 124) {
        throw new Error('Unsupported BMP DIB header');
      }
      check(14, dib);
      if (pixelOffset < 14 + dib || pixelOffset > size) throw new Error('Invalid BMP pixel offset');
      read(14, dib);
      var bmpWidth = le32(4) | 0;
      var bmpHeight = le32(8) | 0;
      var bpp = le16(14);
      if (bmpWidth <= 0 || !bmpHeight || le16(12) !== 1 ||
          (bpp !== 1 && bpp !== 4 && bpp !== 8 && bpp !== 16 && bpp !== 24 && bpp !== 32)) {
        throw new Error('Invalid BMP header metadata');
      }
      // Compression, palettes, masks and pixel data belong to a decoder, not info.
      return { format: 'bmp', width: bmpWidth, height: Math.abs(bmpHeight), bpp: bpp };
    }
    if (bytes[0] !== 255 || bytes[1] !== 216) throw new Error('Unknown image format');
    limit = Math.min(size, 16384);
    var position = 2;
    while (position < limit) {
      read(position++, 1);
      if (bytes[0] !== 255) throw new Error('Invalid JPEG marker');
      do {
        read(position++, 1);
      } while (bytes[0] === 255);
      var marker = bytes[0];
      var sof = marker === 192 || marker === 194;
      // Only length-framed pre-scan markers. SOS, EOI and standalone markers fail.
      if (!sof && marker !== 196 && marker !== 204 && marker !== 219 &&
          marker !== 221 && marker !== 222 && marker !== 223 &&
          !(marker >= 224 && marker <= 239) && marker !== 254) {
        throw new Error('Unsupported JPEG header marker');
      }
      read(position, 2);
      var length = be16(0);
      if (length < 2) throw new Error('Invalid JPEG segment length');
      check(position, length);
      if (sof) {
        if (length < 11 || length > 20) throw new Error('Invalid JPEG SOF length');
        read(position + 2, length - 2);
        var height = be16(1);
        var width = be16(3);
        var components = bytes[5];
        if (bytes[0] !== 8 || !width || !height || components < 1 ||
            components > 4 || length !== 8 + 3 * components) {
          throw new Error('Invalid JPEG SOF metadata');
        }
        for (var i = 0; i < components; i++) {
          var component = 6 + 3 * i;
          var sampling = bytes[component + 1];
          if (!(sampling >> 4) || !(sampling & 15)) throw new Error('Invalid JPEG sampling');
          for (var j = 0; j < i; j++) {
            if (bytes[6 + 3 * j] === bytes[component]) throw new Error('Duplicate JPEG component');
          }
        }
        return { format: 'jpeg', width: width, height: height };
      }
      // Skip payload by its declared length; never scan APP data for markers.
      position += length;
    }
    throw new Error('JPEG dimensions not found within header scan limit');
  } finally {
    fs.closeSync(fd);
  }
}

module.exports = info;
