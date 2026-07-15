#!/usr/bin/env python3
"""Generate the MCU.js 1200x630 Open Graph/social preview card."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
IMG_DIR = ROOT / "static" / "img"
OUT = IMG_DIR / "mcujs-social-card.png"
W, H = 1200, 630

INK = (20, 20, 20)
INK_SOFT = (49, 45, 42)
ORANGE = (242, 109, 61)
ORANGE_DEEP = (213, 86, 43)
SAND_TOP = (248, 244, 238)
SAND_BOTTOM = (239, 226, 210)
PANEL = (22, 22, 22)
PANEL_TEXT = (233, 225, 214)
PANEL_MUTED = (168, 158, 147)
PURPLE = (67, 67, 126)
WHITE = (255, 255, 255)

FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"


def font(size: int, bold: bool = False, mono: bool = False):
    path = FONT_MONO if mono else (FONT_BOLD if bold else FONT_REG)
    return ImageFont.truetype(path, size)


def rounded(draw, box, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def main():
    card = Image.new("RGB", (W, H), SAND_TOP)
    d = ImageDraw.Draw(card)

    # Warm site-matching gradient.
    for y in range(H):
        q = y / (H - 1)
        color = tuple(int(SAND_TOP[i] * (1 - q) + SAND_BOTTOM[i] * q) for i in range(3))
        d.line((0, y, W, y), fill=color)

    # Soft orange focus behind Chippy.
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((760, 70, 1250, 560), fill=(242, 109, 61, 82))
    glow = glow.filter(ImageFilter.GaussianBlur(48))
    card = Image.alpha_composite(card.convert("RGBA"), glow)
    d = ImageDraw.Draw(card)

    # Circuit traces: subtle visual shorthand for microcontrollers.
    trace = (181, 154, 132, 145)
    for y, end_x in [(118, 1035), (198, 1090), (470, 1060), (535, 990)]:
        d.line((690, y, end_x, y), fill=trace, width=3)
        d.ellipse((end_x - 7, y - 7, end_x + 7, y + 7), fill=ORANGE)
    for x, end_y in [(790, 92), (1110, 555)]:
        d.line((x, 40 if end_y < 200 else 410, x, end_y), fill=trace, width=3)
        d.ellipse((x - 7, end_y - 7, x + 7, end_y + 7), fill=ORANGE)

    # Product category pill.
    rounded(d, (66, 58, 450, 105), 23, ORANGE)
    d.text((91, 70), "JAVASCRIPT FOR TINY BOARDS", font=font(21, bold=True), fill=WHITE)

    # Main product statement.
    d.text((62, 125), "mcujs", font=font(105, bold=True), fill=INK)
    d.text((67, 243), "JavaScript runtime", font=font(42, bold=True), fill=INK)
    d.text((67, 293), "for microcontrollers", font=font(42, bold=True), fill=ORANGE_DEEP)

    # Feature chips.
    chips = [
        (67, 367, 246, "REPL OVER USB"),
        (264, 367, 487, "DRAG + DROP JS"),
        (505, 367, 698, "RP2040 / RP2350"),
    ]
    for x0, y0, x1, text in chips:
        rounded(d, (x0, y0, x1, y0 + 42), 21, (255, 255, 255, 170), outline=(210, 192, 174, 255), width=2)
        bbox = d.textbbox((0, 0), text, font=font(16, bold=True))
        tw = bbox[2] - bbox[0]
        d.text(((x0 + x1 - tw) / 2, y0 + 10), text, font=font(16, bold=True), fill=INK_SOFT)

    # Terminal strip echoes the live homepage.
    rounded(d, (67, 447, 702, 572), 20, PANEL)
    rounded(d, (89, 466, 153, 493), 13, ORANGE)
    d.text((101, 472), "REPL", font=font(12, bold=True), fill=WHITE)
    d.text((174, 467), "mcujs session", font=font(17, mono=True), fill=PANEL_MUTED)
    d.text((92, 510), "> GPIO.toggle(25)", font=font(22, mono=True), fill=PANEL_TEXT)
    d.text((411, 510), "Blinking!", font=font(22, mono=True), fill=(255, 165, 102))

    # Chippy is the project, not the framework used to build the docs.
    chippy = Image.open(IMG_DIR / "chippy-icon.png").convert("RGBA")
    chippy.thumbnail((430, 430), Image.Resampling.LANCZOS)
    shadow = Image.new("RGBA", chippy.size, (0, 0, 0, 0))
    shadow_alpha = chippy.getchannel("A").filter(ImageFilter.GaussianBlur(18))
    shadow.putalpha(shadow_alpha)
    shadow_color = Image.new("RGBA", chippy.size, (20, 20, 20, 100))
    shadow_color.putalpha(shadow_alpha.point(lambda a: int(a * 0.42)))
    chippy_x = 755
    chippy_y = 118
    card.alpha_composite(shadow_color, (chippy_x + 10, chippy_y + 18))
    card.alpha_composite(chippy, (chippy_x, chippy_y))

    # Domain mark stays inside common social-crop safe areas.
    rounded(d, (925, 548, 1135, 590), 21, (20, 20, 20, 210))
    domain = "mcujs.org"
    bbox = d.textbbox((0, 0), domain, font=font(18, bold=True))
    d.text((1030 - (bbox[2] - bbox[0]) / 2, 558), domain, font=font(18, bold=True), fill=WHITE)

    card.convert("RGB").save(OUT, format="PNG", optimize=True)
    print(OUT)


if __name__ == "__main__":
    main()
