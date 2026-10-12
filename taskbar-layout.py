#!/usr/bin/env python3
"""media-player / taskbar-layout.py

Measures where the visible elements of a taskbar actually are in a screenshot, so
the widget's placement can be checked with numbers instead of by eye.

Usage:
    python taskbar-layout.py monitor2-taskbar.png -1920
    python taskbar-layout.py monitor2-taskbar.png -1920 --widget-width 390
    python taskbar-layout.py monitor2-taskbar.png -1920 --right-offset 220

`-1920` is the screen x of the capture's left edge (monitor 2 of this machine
starts there, see CONTEXT.md). The script then answers the question the mod's
placement code asks at runtime:

    with this capture, does a widget of `--widget-width` pixels, placed
    `--right-offset` pixels from the right edge of the taskbar, land in the free
    space between the application icons and the system tray?

That matters because the additional displays have no `TrayNotifyWnd` window to
measure: there the mod falls back to `fallbackRightOffset` from the right edge,
and only pixels can tell whether that spot is empty.

Reading the output: the taskbar is translucent, so wallpaper shows through and
can appear as an extra group of bright columns. The application icons are the
widest group left of the tray; the tray is the right-most group.

Capture the taskbar while the widget is NOT on it (another display, or the mod
turned off): otherwise the widget's own pixels become the widest group and the
measurement describes the widget instead of the free space around it.
"""

import argparse

import numpy as np
from PIL import Image

BRIGHTNESS_MARGIN = 6

# Icons inside one group of pinned applications stand ~20 px apart, so nearby
# runs of bright columns have to be merged before the group can be recognised.
# Wallpaper showing through the translucent taskbar produces narrow runs too,
# which is why width - not brightness - decides what is the icon block.
MERGE_GAP = 30


def groups_of_content(columns):
    """Runs of columns brighter than the taskbar's own background, merged."""
    background = float(np.median(columns))
    bright = columns > background + BRIGHTNESS_MARGIN

    runs = []
    start = None
    for index, flag in enumerate(bright):
        if flag and start is None:
            start = index
        elif not flag and start is not None:
            runs.append((start, index - 1))
            start = None
    if start is not None:
        runs.append((start, len(bright) - 1))

    merged = []
    for run in runs:
        if merged and run[0] - merged[-1][1] <= MERGE_GAP:
            merged[-1] = (merged[-1][0], run[1])
        else:
            merged.append(run)
    return background, [r for r in merged if r[1] - r[0] > 3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="PNG of one taskbar strip")
    parser.add_argument(
        "screen_left",
        type=int,
        help="screen x of the capture's left edge, e.g. -1920 for a left display",
    )
    parser.add_argument("--widget-width", type=int, default=390)
    parser.add_argument(
        "--right-offset",
        type=int,
        default=220,
        help="the mod's fallbackRightOffset: distance from the right edge",
    )
    args = parser.parse_args()

    image = Image.open(args.capture).convert("RGB")
    array = np.asarray(image).astype(int)
    columns = array.mean(axis=2).mean(axis=0)
    width = image.size[0]
    screen_right = args.screen_left + width

    background, runs = groups_of_content(columns)
    print(f"{args.capture}: {width} x {image.size[1]}, background ~ {background:.1f}")
    print(f"screen span: {args.screen_left}..{screen_right}")
    print("groups of content (screen coordinates):")
    for start, end in runs:
        print(
            f"  {start + args.screen_left:6d}..{end + args.screen_left:6d}"
            f"  width {end - start + 1:4d}"
        )

    if not runs:
        print("no content found; is this really a taskbar capture?")
        return

    tray = runs[-1] if runs[-1][1] > width * 0.75 else None
    before_tray = [r for r in runs if tray is None or r[1] < tray[0]]
    icons = max(before_tray, key=lambda r: r[1] - r[0]) if before_tray else None

    if icons:
        print(
            f"application icons: {icons[0] + args.screen_left}"
            f"..{icons[1] + args.screen_left}"
        )
    if tray:
        print(f"system tray     : {tray[0] + args.screen_left}..{tray[1] + args.screen_left}")
    if icons and tray:
        gap = tray[0] - icons[1] - 1
        print(f"free gap between them: {gap} px")

    right_edge = screen_right - args.right_offset
    left_edge = right_edge - args.widget_width
    print(
        f"widget at right offset {args.right_offset}: "
        f"{left_edge}..{right_edge} ({args.widget_width} px)"
    )

    problems = []
    if icons:
        icons_left = icons[0] + args.screen_left
        icons_right = icons[1] + args.screen_left
        print(f"gap from the icons to the widget: {left_edge - icons_right} px")
        if left_edge < icons_right:
            problems.append(f"it covers the application icons by {icons_right - left_edge} px")
    if tray:
        tray_left = tray[0] + args.screen_left
        print(f"gap from the widget to the tray: {tray_left - right_edge} px")
        if right_edge > tray_left:
            problems.append(f"it reaches into the system tray by {right_edge - tray_left} px")
    if left_edge < args.screen_left:
        problems.append("it does not fit on the taskbar at all")

    if problems:
        print("VERDICT: does not fit cleanly:")
        for problem in problems:
            print(f"  - {problem}")
    else:
        print("VERDICT: fits in the free space")


if __name__ == "__main__":
    main()
