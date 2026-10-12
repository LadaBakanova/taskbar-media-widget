"""Measure the glyph inside each of the three buttons in a preview render.

The button faces are a faint white overlay (#ffffff17) on the panel, the glyphs
are #ffffff at full or 0.6 alpha, so only clearly bright pixels belong to the
glyph. The measured box is reported in design units (the render is 96 DPI, so
one pixel is one design unit).
"""

import os
import sys
from PIL import Image

OUT = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "preview", "out"))

BUTTONS = {
    "prev": (367.0, 395.0),
    "play": (398.5, 426.5),
    "next": (431.0, 459.0),
}
TOP = 8
BOTTOM = 36
THRESHOLD = 90  # luminance of the most opaque glyph pixels


def box(image, x0, x1):
    pixels = image.load()
    xs, ys = [], []
    for y in range(image.height):
        for x in range(x0, x1):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            if max(r, g, b) >= THRESHOLD:
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "widget-playing.png"
    image = Image.open(os.path.join(OUT, name)).convert("RGBA")
    print("== %s" % name)
    for button, (left, right) in BUTTONS.items():
        found = box(image, int(left) - 1, int(right) + 1)
        if not found:
            print("  %-4s : nothing bright" % button)
            continue
        bx0, by0, bx1, by1 = found
        print("  %-4s : x %.2f..%.2f (w %.2f, centre %.2f, button centre %.2f)"
              "  y %.2f..%.2f (h %.2f, centre %.2f)"
              % (button, bx0, bx1, bx1 - bx0, (bx0 + bx1) / 2,
                 (left + right) / 2, by0, by1, by1 - by0, (by0 + by1) / 2))


if __name__ == "__main__":
    main()
