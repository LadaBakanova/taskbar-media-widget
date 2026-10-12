"""Crop the button area out of the preview renders and magnify it.

Usage: python compare-icons.py <out dir> [scale]
Writes icons-<state>-zoom.png next to the renders.
"""

import os
import sys
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "preview", "out")
OUT = os.path.normpath(OUT)

# x = 360 catches the left edge of the prev button at the design's width.
BOX = (360, 4, 466, 38)


def zoom(src, dst, scale):
    image = Image.open(src).convert("RGBA")
    crop = image.crop(BOX)
    small = crop.resize((crop.width * scale, crop.height * scale), Image.NEAREST)
    small.save(dst)
    print("%s -> %s (%dx%d)" % (src, dst, small.width, small.height))


def main():
    scale = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    for state in ("playing", "paused"):
        zoom(os.path.join(OUT, "widget-%s.png" % state),
             os.path.join(OUT, "icons-%s-zoom.png" % state), scale)
        zoom(os.path.join(OUT, "icons-before", "widget-%s.png" % state),
             os.path.join(OUT, "icons-before-%s-zoom.png" % state), scale)


if __name__ == "__main__":
    main()
