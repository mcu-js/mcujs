"""Prepare a build-local copy of the pinned picojpeg, never modify the SDK.

Green must remain signed until both chroma contributions are accumulated.
The upstream byte intermediate clips blue's negative Cb contribution before
Cr adds back green (e.g. blue becomes RGB 1,14,252 instead of 1,0,252).
This costs 512 additional static bytes, not an image-sized allocation.
"""
from pathlib import Path
import hashlib
import re
import sys

SOURCE_SHA256 = 'e8cbf352a9d9aa6074259d53d9de3341b883fa52380fa94d83ce7e47b91d3a29'
HEADER_SHA256 = 'f0a247e2eac6ecb2d2c6f94d29b8f99eca4977c217e5d76ca1db9f4db162414c'

def prepare(source, output):
    source, output = Path(source), Path(output)
    if source.resolve() == output.resolve():
        raise ValueError('picojpeg output must not overwrite the SDK source')
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != SOURCE_SHA256:
        raise ValueError('unreviewed picojpeg source version')
    if hashlib.sha256(source.with_suffix('.h').read_bytes()).hexdigest() != HEADER_SHA256:
        raise ValueError('unreviewed picojpeg header version')
    text = raw.decode().replace('\r\n', '\n')
    text = text.replace('static uint8 gMCUBufG[256];',
                        'static int16 gMCUBufG[256];\nstatic uint8 gMCUBufGOutput[256];')
    text = text.replace('uint8* pDstG = gMCUBufG', 'int16* pDstG = gMCUBufG')
    text = text.replace('uint8* pGDst = gMCUBufG', 'int16* pGDst = gMCUBufG')
    text, count = re.subn(r'subAndClamp\(((?:pDstG|gMCUBufG)\[\d+\]), (c[br]G)\)',
                          r'((int16)(\1) - (\2))', text)
    if count != 36:
        raise ValueError('unexpected picojpeg green conversion layout')
    # Avoid unsequenced pointer increment/read in the upstream 4:4:4 path.
    text, count = re.subn(r'\*(pDst[RGB])\+\+ = ([^;\n]+);',
                          r'*\1 = \2; ++\1;', text)
    if count != 4:
        raise ValueError('unexpected picojpeg 4:4:4 conversion layout')
    text = text.replace('   gNumMCUSRemainingX--;',
                        '   { uint16 j; for (j = 0; j < 256; j++)\n'
                        '      gMCUBufGOutput[j] = clamp(gMCUBufG[j]); }\n'
                        '   gNumMCUSRemainingX--;')
    text = text.replace('pInfo->m_pMCUBufG = gMCUBufG;',
                        'pInfo->m_pMCUBufG = gMCUBufGOutput;')
    # Marker lookahead is allowed, but synthetic FF bytes are not entropy.
    # Count genuine buffered entropy bits; debit only actual symbol reads, not
    # the initial/restart bit-buffer priming. Checking after refill permits
    # valid 16-bit reads when the bit buffer initially holds only eight bits.
    text = text.replace('static uint8 gBitsLeft;',
                        'static uint8 gBitsLeft;\nstatic int16 gEntropyBits;\n'
                        'static uint8 gCountEntropy;')
    text = text.replace('static uint8 gCallbackStatus;',
                        'static uint8 gCallbackStatus;\n'
                        'static void consumeEntropy(uint8 n) {\n'
                        '   if (gCountEntropy && (gEntropyBits -= n) < 0) {\n'
                        '      gEntropyBits = 0; gCallbackStatus = PJPG_DECODE_ERROR;\n'
                        '   }\n}\n')
    text = text.replace('         stuffChar(0xFF);',
                        '         stuffChar(0xFF);\n         return c;')
    text = text.replace('   return c;\n}\n//------------------------------------------------------------------------------\nstatic uint16 getBits',
                        '   if (FFCheck) gEntropyBits += 8;\n   return c;\n}\n'
                        '//------------------------------------------------------------------------------\nstatic uint16 getBits')
    text = text.replace('   return getBits(numBits, 1);',
                        '   uint16 result = getBits(numBits, 1);\n'
                        '   consumeEntropy(numBits);\n   return result;')
    text = text.replace('   gBitsLeft--;\n   gBitBuf <<= 1;',
                        '   gBitsLeft--;\n   gBitBuf <<= 1;\n   consumeEntropy(1);')
    prime = '   gBitsLeft = 8;\n   getBits2(8);\n   getBits2(8);'
    if text.count(prime) != 2:
        raise ValueError('unexpected picojpeg entropy priming layout')
    text = text.replace(prime,
                        '   gEntropyBits = 0; gCountEntropy = 0;\n' + prime +
                        '\n   gCountEntropy = 1;')
    # Propagate parser failures instead of resuming at a later valid marker.
    for parser in ('readDHTMarker', 'readDQTMarker', 'readDRIMarker', 'skipVariableMarker'):
        old = '            ' + parser + '();'
        if text.count(old) != 1:
            raise ValueError('unexpected picojpeg marker dispatch')
        text = text.replace(old, '            uint8 status = ' + parser + '();\n'
                                 '            if (status) return status;')
    text = text.replace('if (left < 2)\n      return PJPG_BAD_DHT_MARKER;',
                        'if (left < 20)\n      return PJPG_BAD_DHT_MARKER;')
    text = text.replace('      index = (uint8)getBits1(8);',
                        '      if (left < 18) return PJPG_BAD_DHT_MARKER;\n'
                        '      index = (uint8)getBits1(8);')
    text = text.replace('      if (count > getMaxHuffCodes(tableIndex))',
                        '      if (!count || count > getMaxHuffCodes(tableIndex) || left < 17U + count)')
    old = '      for (i = 0; i < count; i++)\n         pHuffVal[i] = (uint8)getBits1(8);'
    new = """      {
         unsigned long slots = 1;
         for (i = 0; i < 16; i++) {
            slots *= 2;
            if (bits[i] >= slots) return PJPG_BAD_DHT_COUNTS;
            slots -= bits[i]; /* reserve the forbidden all-one padding code */
         }
      }
      for (i = 0; i < count; i++) {
         uint8 value = (uint8)getBits1(8);
         if (tableIndex < 2 ? value > 11 :
             ((value & 15) > 10 || (!(value & 15) && value != 0 && value != 0xF0)))
            return PJPG_BAD_DHT_MARKER;
         pHuffVal[i] = value;
      }"""
    if text.count(old) != 1:
        raise ValueError('unexpected picojpeg Huffman table reader')
    text = text.replace(old, new)
    text = text.replace('      if (i == 16)\n         return 0;',
                        '      if (i == 16) { gCallbackStatus = PJPG_DECODE_ERROR; return 0; }')
    text = text.replace('if (left < 2)\n      return PJPG_BAD_DQT_MARKER;',
                        'if (left < 67)\n      return PJPG_BAD_DQT_MARKER;')
    text = text.replace('      if (n > 1)\n         return PJPG_BAD_DQT_TABLE;',
                        '      if (n > 1)\n         return PJPG_BAD_DQT_TABLE;\n'
                        '      if (prec || left < 65) return PJPG_BAD_DQT_LENGTH;')
    text = text.replace('         uint16 temp = getBits1(8);',
                        '         uint16 temp = getBits1(8);\n'
                        '         if (!temp) return PJPG_BAD_DQT_TABLE;')
    # The decoder already tracks genuine buffered entropy bits. At each scan
    # boundary only zero to seven one-bits may remain, followed immediately by
    # the expected marker (allowing legal FF marker fill, not arbitrary bytes).
    start = text.index('static uint8 processRestart(void)')
    reset = text.index('   // Reset each component', start)
    text = text[:start] + """static uint8 finishEntropy(uint8 expected)
{
   uint8 c;
   if (gCallbackStatus || gEntropyBits < 0 || gEntropyBits > 7)
      return PJPG_DECODE_ERROR;
   if (gEntropyBits && (gBitBuf >> (16 - gEntropyBits)) != ((1U << gEntropyBits) - 1U))
      return PJPG_DECODE_ERROR;
   if (getChar() != 0xFF) return PJPG_DECODE_ERROR;
   do { c = getChar(); } while (c == 0xFF && !gCallbackStatus);
   return gCallbackStatus ? gCallbackStatus : (c == expected ? 0 : PJPG_DECODE_ERROR);
}
static uint8 processRestart(void)
{
   uint8 status = finishEntropy((uint8)(gNextRestartNum + M_RST0));
   if (status) return status;

""" + text[reset:]
    text = text.replace('   gNumMCUSRemainingX--;',
                        '   if (gNumMCUSRemainingX == 1 && gNumMCUSRemainingY == 1) {\n'
                        '      status = finishEntropy(M_EOI);\n'
                        '      if (status) return status;\n'
                        '   }\n   gNumMCUSRemainingX--;')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: prepare-picojpeg.py SDK/picojpeg.c BUILD/picojpeg.c')
    prepare(*sys.argv[1:])
