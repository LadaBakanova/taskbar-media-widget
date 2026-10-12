"""Emit the four icons/*.svg path strings as C++ string literals.

The widget's own SvgPathReader understands the same command subset the icons
use (M/m, L/l, H/h, V/v, C/c, Z/z), so the `d` attributes can go into the
source verbatim - only the line wrapping has to be added.
"""

import os
import re

ICONS = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "icons"))

WIDTH = 72


def chunks(text):
    return [text[i:i + WIDTH] for i in range(0, len(text), WIDTH)]


def emit(name, text):
    lines = ['constexpr char %s[] =' % name]
    parts = chunks(text)
    for n, part in enumerate(parts):
        tail = ";" if n == len(parts) - 1 else ""
        head = "    " if n else "    "
        lines.append('%s"%s"%s' % (head, part, tail))
    return "\n".join(lines)


def main():
    for name in ("back", "next", "play", "pause"):
        svg = open(os.path.join(ICONS, name + ".svg"), encoding="utf-8").read()
        ds = re.findall(r'd="([^"]+)"', svg)
        print("// %s.svg" % name)
        for n, d in enumerate(ds):
            label = "k%sPath%d" % (name.capitalize(), n) if len(ds) > 1 \
                else "k%sPath" % name.capitalize()
            print(emit(label, d))
        print()


if __name__ == "__main__":
    main()
