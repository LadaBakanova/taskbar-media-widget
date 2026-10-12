"""Crop the live widget out of a full-screen capture and magnify it.

Usage: python crop-live.py <capture.png> <left> <top> <width> <height> [scale]
"""

import os
import sys
from PIL import Image

OUT = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "preview", "out"))


def main():
    src = os.path.join(OUT, sys.argv[1])
    left, top, width, height = (int(v) for v in sys.argv[2:6])
    scale = int(sys.argv[6]) if len(sys.argv) > 6 else 3
    image = Image.open(src).convert("RGBA")
    crop = image.crop((left, top, left + width, top + height))
    zoomed = crop.resize((crop.width * scale, crop.height * scale), Image.NEAREST)
    dst = os.path.join(OUT, "live-widget-zoom.png")
    zoomed.save(dst)
    print("%s -> %s (%dx%d)" % (src, dst, zoomed.width, zoomed.height))


if __name__ == "__main__":
    main()
