# media-player / measure-blur.py
# Reads the two pictures blur-probe.exe takes and reports, per row:
#
#   * the colour in the middle of each of the four backdrop bands, which is what
#     the glass recipe does to what is behind the window. The reference row (no
#     glass at all) shows the raw bands; the taskbar's own pill recipe - a blur
#     plus a 15 % tint - would keep roughly 85 % of that brightness;
#   * how dark the glass is compared with the reference, as a percentage;
#   * whether the top-left corner of the window is glass (square corner) or the
#     backdrop showing through (rounded corner). The rows paint nothing
#     themselves, so the corner pixel answers the corner question directly.
#
# Usage: python measure-blur.py [early.png] [late.png]

import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROW_HEIGHT = 42
ROW_GAP = 10
MARGIN = 24
ROWS = [
    "1 reference: no glass, raw bands",
    "2 acrylic modern, theme tint",
    "3 acrylic modern, white tint",
    "4 acrylic old recipe, theme tint",
    "5 host backdrop, theme tint",
    "6 blur behind, no tint",
    "7 acrylic + window region 10",
    "8 acrylic + DWM rounding",
    "9 acrylic on a separate backdrop + region",
]


def row_top(index: int) -> int:
    return MARGIN + index * (ROW_HEIGHT + ROW_GAP)


def patch(pixels: np.ndarray, top: int, left: int, size: int = 16) -> np.ndarray:
    crop = pixels[top : top + size, left : left + size]
    return np.median(crop.reshape(-1, pixels.shape[2]), axis=0)


def report(path: Path) -> None:
    image = Image.open(path).convert("RGB")
    pixels = np.asarray(image, dtype=np.float64)
    band = image.width / 4.0
    # Middle of the row, away from the corners.
    offset = ROW_HEIGHT // 2 - 8

    colours = []
    corners = []
    for index in range(len(ROWS)):
        top = row_top(index) + offset
        samples = []
        for band_index in range(3):
            left = int(band * band_index + band / 2) - 8
            samples.append(patch(pixels, top, left))
        colours.append(samples)
        # 1 px inside the window's own top-left corner.
        corners.append(pixels[row_top(index) + 1, MARGIN + 1])

    reference = colours[0]
    print(f"\n{path.name}  {image.width}x{image.height}")
    print(f"{'row':<44}{'red':>14}{'green':>14}{'blue':>14}{'bright':>8}{'corner':>22}")
    print("-" * 118)
    for index, name in enumerate(ROWS):
        red, green, blue = colours[index]
        factors = [
            float(np.mean(colours[index][band_index]) / np.mean(reference[band_index]))
            for band_index in range(3)
            if np.mean(reference[band_index]) > 1
        ]
        brightness = 100.0 * float(np.mean(factors)) if factors else float("nan")
        corner = corners[index]
        reference_corner = corners[0]
        if float(np.abs(corner - reference_corner).max()) < 25:
            corner_verdict = "pattern shows: rounded"
        else:
            corner_verdict = "glass: square corner"
        fmt = lambda c: f"({int(c[0])},{int(c[1])},{int(c[2])})"
        print(
            f"{name:<44}{fmt(red):>14}{fmt(green):>14}{fmt(blue):>14}"
            f"{brightness:>7.0f}%{corner_verdict:>22}"
        )
    print(
        "\nHow to read it: the taskbar's own pills are a blur with a 15 % tint, so a\n"
        "recipe that matches them keeps the band colours near 85 % of the reference\n"
        "and gives the corner back to the backdrop."
    )


def main() -> int:
    paths = sys.argv[1:] or [
        "preview/out/blur-probe.png",
        "preview/out/blur-probe-late.png",
    ]
    missing = 0
    for raw in paths:
        path = Path(raw)
        if not path.is_file():
            print(f"no such file: {path}")
            missing += 1
            continue
        report(path)
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
