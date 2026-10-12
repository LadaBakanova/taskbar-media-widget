"""Convert the four icons/*.svg files into the C++ literals the widget uses.

Reads the `d` attributes of every <path> in each file, converts any SVG arc
(A/a) into cubic Beziers so the widget's small path reader can handle it, and
prints a ready-to-paste C++ snippet plus the path bounding boxes.
"""

import re
import math
import os
import sys

ICONS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "icons")
ICONS = os.path.normpath(ICONS)

TOKEN = re.compile(
    r"([MmLlHhVvCcSsQqTtAaZz])|(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"
)


def tokenize(d):
    out = []
    for cmd, num in TOKEN.findall(d):
        out.append(cmd if cmd else float(num))
    return out


def arc_to_beziers(x1, y1, rx, ry, phi, large, sweep, x2, y2):
    """SVG endpoint arc -> list of (c1x, c1y, c2x, c2y, x, y), absolute."""
    if rx == 0 or ry == 0:
        return [(x1, y1, x2, y2, x2, y2)]
    rx, ry = abs(rx), abs(ry)
    phi = math.radians(phi)
    cos_p, sin_p = math.cos(phi), math.sin(phi)

    dx2, dy2 = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    x1p = cos_p * dx2 + sin_p * dy2
    y1p = -sin_p * dx2 + cos_p * dy2
    lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if lam > 1:
        s = math.sqrt(lam)
        rx, ry = rx * s, ry * s
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    coef = math.sqrt(max(0.0, num / den))
    if large == sweep:
        coef = -coef
    cxp = coef * rx * y1p / ry
    cyp = -coef * ry * x1p / rx
    cx = cos_p * cxp - sin_p * cyp + (x1 + x2) / 2.0
    cy = sin_p * cxp + cos_p * cyp + (y1 + y2) / 2.0

    def angle(ux, uy, vx, vy):
        dot = ux * vx + uy * vy
        n = math.hypot(ux, uy) * math.hypot(vx, vy)
        a = math.acos(max(-1.0, min(1.0, dot / n)))
        return -a if (ux * vy - uy * vx) < 0 else a

    theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                   (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dtheta > 0:
        dtheta -= 2 * math.pi
    elif sweep and dtheta < 0:
        dtheta += 2 * math.pi

    segments = max(1, int(math.ceil(abs(dtheta) / (math.pi / 2))))
    step = dtheta / segments
    k = 4.0 / 3.0 * math.tan(step / 4.0)
    result = []
    t = theta1
    px, py = x1, y1
    for _ in range(segments):
        t2 = t + step
        cos1, sin1 = math.cos(t), math.sin(t)
        cos2, sin2 = math.cos(t2), math.sin(t2)

        def point(c, s):
            return (cos_p * rx * c - sin_p * ry * s + cx,
                    sin_p * rx * c + cos_p * ry * s + cy)

        def tangent(c, s):
            return (-cos_p * rx * s - sin_p * ry * c,
                    -sin_p * rx * s + cos_p * ry * c)

        ex, ey = point(cos2, sin2)
        t1x, t1y = tangent(cos1, sin1)
        t2x, t2y = tangent(cos2, sin2)
        result.append((px + k * t1x, py + k * t1y,
                       ex - k * t2x, ey - k * t2y, ex, ey))
        px, py, t = ex, ey, t2
    return result


def normalize(d):
    """Absolutise every command and expand arcs; returns (commands, bbox)."""
    toks = tokenize(d)
    i = 0
    cmd = None
    cur = (0.0, 0.0)
    start = (0.0, 0.0)
    out = []
    xs, ys = [], []

    def track(x, y):
        xs.append(x)
        ys.append(y)

    def num():
        nonlocal i
        v = toks[i]
        i += 1
        return v

    def is_num():
        return i < len(toks) and not isinstance(toks[i], str)

    while i < len(toks):
        if isinstance(toks[i], str):
            cmd = toks[i]
            i += 1
        if cmd is None:
            raise ValueError("path does not start with a command")
        if cmd in "Zz":
            out.append(("Z",))
            cur = start
            continue
        if cmd in "Mm":
            x, y = num(), num()
            if cmd == "m":
                x, y = cur[0] + x, cur[1] + y
            cur = start = (x, y)
            track(x, y)
            out.append(("M", x, y))
            cmd = "L" if cmd == "M" else "l"
            continue
        if cmd in "Ll":
            while is_num():
                x, y = num(), num()
                if cmd == "l":
                    x, y = cur[0] + x, cur[1] + y
                cur = (x, y)
                track(x, y)
                out.append(("L", x, y))
            continue
        if cmd in "Hh":
            while is_num():
                x = num()
                if cmd == "h":
                    x += cur[0]
                cur = (x, cur[1])
                track(*cur)
                out.append(("L", cur[0], cur[1]))
            continue
        if cmd in "Vv":
            while is_num():
                y = num()
                if cmd == "v":
                    y += cur[1]
                cur = (cur[0], y)
                track(*cur)
                out.append(("L", cur[0], cur[1]))
            continue
        if cmd in "Cc":
            while is_num():
                x1, y1, x2, y2, x, y = (num() for _ in range(6))
                if cmd == "c":
                    x1, y1 = cur[0] + x1, cur[1] + y1
                    x2, y2 = cur[0] + x2, cur[1] + y2
                    x, y = cur[0] + x, cur[1] + y
                out.append(("C", x1, y1, x2, y2, x, y))
                track(x1, y1)
                track(x2, y2)
                cur = (x, y)
                track(x, y)
            continue
        raise ValueError("unsupported command %r" % cmd)

    return out, (min(xs), min(ys), max(xs), max(ys))


def cpp(commands, indent="    "):
    """Emit the commands as C++ string-literal chunks."""
    text = []
    for c in commands:
        if c[0] == "Z":
            text.append("z")
        else:
            vals = " ".join(("%g" % round(v, 5)) for v in c[1:])
            text.append(c[0].lower() + vals)
    return text


def fmt_path(commands, name):
    """Emit as a C++ constexpr char k...[] = "..."; block."""
    parts = []
    for c in commands:
        if c[0] == "Z":
            parts.append("z")
        else:
            vals = " ".join(("%g" % round(v, 4)) for v in c[1:])
            parts.append(c[0].lower() + vals)
    joined = "".join(parts)
    chunks = [joined[i:i + 68] for i in range(0, len(joined), 68)]
    lines = []
    for n, ch in enumerate(chunks):
        prefix = "constexpr char %s[] =" % name if n == 0 else "    "
        lines.append('%s\n    "%s"' % (prefix, ch))
    lines[-1] += ";"
    return "\n".join(lines)


def main():
    names = ["back", "next", "play", "pause"]
    for name in names:
        path = os.path.join(ICONS, name + ".svg")
        svg = open(path, encoding="utf-8").read()
        ds = re.findall(r'd="([^"]+)"', svg)
        print("=" * 72)
        print("%s.svg: %d path(s)" % (name, len(ds)))
        for n, d in enumerate(ds):
            commands, bbox = normalize(d)
            print("  path %d bbox x=[%.4f, %.4f] w=%.4f  y=[%.4f, %.4f] h=%.4f"
                  % (n, bbox[0], bbox[2], bbox[2] - bbox[0],
                     bbox[1], bbox[3], bbox[3] - bbox[1]))
            print("    center x=%.4f y=%.4f" % ((bbox[0] + bbox[2]) / 2,
                                               (bbox[1] + bbox[3]) / 2))
            print("    commands: %s" % " ".join(
                c[0] if c[0] != "C" else "C" for c in commands))
            print(fmt_path(commands, "k%s%d" % (name.capitalize(), n)))
    print("=" * 72)


if __name__ == "__main__":
    main()
