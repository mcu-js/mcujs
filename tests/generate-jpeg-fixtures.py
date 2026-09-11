"""Regenerate tiny JPEG fixtures/oracles with Pillow, not picojpeg.
Run manually with an existing Pillow environment; native runner needs no Pillow.
Smooth RGB ramps avoid libjpeg fancy-upsampling boundary differences; allow 5
levels for picojpeg integer IDCT/color conversion. Gray permits 2 levels.
"""
from pathlib import Path
from PIL import Image, __version__
root = Path(__file__).with_name('jpeg_fixtures')
root.mkdir(exist_ok=True)
for name, mode, size, sub, progressive in [
    ('rgb444', 'RGB', (17, 19), 0, False),
    ('rgb422', 'RGB', (17, 19), 1, False),
    ('rgb420', 'RGB', (17, 19), 2, False),
    ('gray', 'L', (17, 19), 0, False),
    ('one', 'L', (1, 1), 0, False),
    ('progressive', 'RGB', (17, 19), 2, True),
    ('oversize', 'L', (321, 1), 0, False),
    ('restart', 'RGB', (17, 19), 2, False),
    ('restartgray', 'L', (1, 9), 0, False),
    ('blue444', 'RGB', (17, 19), 0, False),
    ('blue420', 'RGB', (17, 19), 2, False),
]:
    image = Image.new(mode, size)
    for y in range(size[1]):
        for x in range(size[0]):
            image.putpixel((x,y), (40+x*3, 60+y*3, 80+x+y) if mode == 'RGB' else (128 if name in ('one', 'restartgray') else 20+x*5+y*4) % 256)
    path = root/(name+'.jpg')
    if name.startswith('blue'):
        image.paste((0, 0, 255), (0, 0, *size))
    image.save(path, quality=95, subsampling=sub, progressive=progressive,
               restart_marker_blocks=1 if name in ('restart', 'restartgray') else 0)
    if name not in ('oversize', 'progressive'):
        with Image.open(path) as decoded:
            (root/(name+'.rgb')).write_bytes(decoded.convert('RGB').tobytes())
print('Generated independent fixtures with Pillow', __version__)
