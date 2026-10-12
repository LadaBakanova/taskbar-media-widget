#!/usr/bin/env python3
"""media-player / check-capture.py

Reads a capture of the widget and reports what was actually drawn, so claims
about the widget can be checked against pixels instead of impressions.

Usage:
    python check-capture.py widget-screen.png

The widget has two ways of looking:

* the design's own panel (design.pen: 466 x 42, #1a1a1a, corner radius 8), which
  is what the mod draws when it is not following the taskbar's theme;
* the taskbar's surface, taken from the theme selected in the Windows 11 Taskbar
  Styler mod - a translucent background, the theme's corner radius and border.

The second one has no fixed colours, so every check below is relative to the panel
colour sampled from the capture itself: each element only has to stand out from
the panel it sits on, and the geometry has to be the layout's either way.

The layout is the design less the shifts the widget carries: the 35 px the
right-hand group was pulled towards the title and the artist in 2.8.0
(design::kCompactShift), the 2 px the time's right edge sits before the design's
own since 2.8.8 (design::kTimeShift - the bar is as long as the widest time,
"00:00:00 / 00:00:00", needs next to the meter) and the 10 px the buttons were
pulled towards the time in 2.8.4 (design::kButtonShift):

    panel        419 x 42, rounded corners, the fill is whatever the theme says
    cover        32 x 32 at (4.5, 5), rounded, radius 5
    progress     135 x 4 at (170.5, 12) - 2 shorter than the design's own 137
    time         Inter 10, right edge at 305.5
    meter        18 strokes from x = 171.5, pitch 1.5, bottom y = 34, ends on 197
    buttons      28 x 28 at y = 8, x = 320 / 351.5 / 384
"""

import sys
from collections import Counter

from PIL import Image

DESIGN_PANEL = (26, 26, 26)   # #1a1a1a, the design's own panel
BAR = dict(left=170.5, top=12.0, width=135.0, height=4.0)
COVER = dict(left=4.5, top=5.0, width=32.0, height=32.0)
BUTTONS = [320.0, 351.5, 384.0]
BUTTON_SIZE = 28.0
BUTTON_TOP = 8.0
METER_FIRST_X = 171.5
METER_PITCH = 1.5
METER_COUNT = 18
METER_BOTTOM = 34.0
PANEL_PROBE = (60, 21)        # inside the panel, above the text
PANEL_SIZE = (419, 42)


def close(pixel, colour, tolerance=10):
    return all(abs(int(a) - b) <= tolerance for a, b in zip(pixel[:3], colour))


def distance(pixel, colour):
    return max(abs(int(a) - b) for a, b in zip(pixel[:3], colour))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "widget-screen.png"
    img = Image.open(path).convert("RGBA")
    width, height = img.size
    px = img.load()

    print(f"capture : {path}")
    print(f"size    : {width} x {height}")

    panel = px[PANEL_PROBE[0], PANEL_PROBE[1]][:3]
    themed = not close(panel, DESIGN_PANEL, 6)
    print(f"panel   : #{panel[0]:02x}{panel[1]:02x}{panel[2]:02x}"
          f"{' (a themed, translucent panel)' if themed else ' (the design panel)'}")
    print()

    # With a translucent panel the colour behind it varies across the widget, so
    # each element is compared with the panel column above it (row 4 is panel only:
    # the cover starts at y = 5, the buttons at y = 8).
    reference = {x: px[x, 4][:3] for x in range(width)}

    def stands_out(x, y, threshold=8):
        return distance(px[x, y], reference[x]) > threshold

    checks = []

    def check(name, expected, actual, ok):
        checks.append(ok)
        print(f"  {'ok  ' if ok else 'FAIL'} {name:26s} expected {expected}, found {actual}")

    check("panel size", f"{PANEL_SIZE[0]} x {PANEL_SIZE[1]}", f"{width} x {height}", (width, height) == PANEL_SIZE)
    if not themed:
        # A screen capture shows the taskbar through the rounded corner and a
        # window capture shows it transparent; on a translucent panel the corner
        # and the panel look alike, so this is only checked for the design panel.
        check("corner (0, 0) rounded", "not the panel fill", tuple(px[0, 0][:3]),
              distance(px[0, 0], panel) > 6)

    # Progress bar: the first and last column of the bar's row that is not the
    # panel, looked for only where the design puts the bar.
    row = int(BAR["top"] + BAR["height"] / 2)
    scan = range(int(BAR["left"]) - 6, int(BAR["left"] + BAR["width"]) + 7)
    bar_pixels = [x for x in scan if stands_out(x, row)]
    if bar_pixels:
        left, right = min(bar_pixels), max(bar_pixels)
        check("progress bar left", BAR["left"], left, abs(left - BAR["left"]) <= 1)
        check("progress bar right", BAR["left"] + BAR["width"], right + 1,
              abs((right + 1) - (BAR["left"] + BAR["width"])) <= 1)
        check("track stands out", "not the panel fill",
              tuple(px[right - 5, row][:3]), stands_out(right - 5, row))
    else:
        check("progress bar", "visible", "not found", False)

    # Level meter: count the strokes, looking for anything that stands out from the
    # panel on the rows the strokes occupy. The strokes are 1 unit wide on a 1.5
    # unit pitch, so what is sampled is the middle of each stroke rather than a
    # fixed step that would land in the half unit gaps.
    strokes = 0
    for index in range(METER_COUNT):
        centre = int(METER_FIRST_X + METER_PITCH * index + 0.5)
        column = range(int(METER_BOTTOM) - 11, int(METER_BOTTOM))
        if any(stands_out(centre, y, 20) for y in column):
            strokes += 1
    check("level meter strokes", METER_COUNT, strokes, strokes >= METER_COUNT - 2)

    # Buttons: their fill is a flat wash over whatever the panel is.
    for index, left in enumerate(BUTTONS):
        probe = (int(left + BUTTON_SIZE / 2), int(BUTTON_TOP + 3))
        check(f"button {index} stands out at x={left:.0f}", "not the panel fill",
              tuple(px[probe[0], probe[1]][:3]), stands_out(probe[0], probe[1]))

    # Cover: the slot is drawn only when the player publishes artwork.
    region = [px[x, y] for x in range(int(COVER["left"]) + 3,
                                      int(COVER["left"] + COVER["width"]) - 3)
              for y in range(int(COVER["top"]) + 3, int(COVER["top"] + COVER["height"]) - 3)]
    distinct = len(Counter(c[:3] for c in region))
    drawn = sum(1 for c in region if distance(c, panel) > 6)
    print()
    if not drawn:
        print("  cover slot: panel colour everywhere -> no cover (the player "
              "published none, or decoding kept failing)")
    elif distinct > 400:
        print(f"  cover slot: {distinct} distinct colours -> a cover is drawn")
    elif distinct > 30:
        print(f"  cover slot: {distinct} distinct colours -> something is drawn, "
              f"check it visually")
    else:
        print(f"  cover slot: flat colour, {distinct} distinct values -> the slot "
              f"is filled but holds no picture")

    print()
    failed = [c for c in checks if not c]
    if failed:
        print(f"RESULT: {len(failed)} of {len(checks)} checks disagree with the design")
        sys.exit(1)
    print(f"RESULT: all {len(checks)} checks match the design")


if __name__ == "__main__":
    main()
