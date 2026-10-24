#!/usr/bin/env python3
"""Print the centre "x y" of the largest FluentWinUI3 accent-coloured (light theme #005FB8)
area inside a screen region. Used by gui-acceptance.sh to press highlighted dialog buttons.
Usage: find-accent.py <png> <x0> <y0> <x1> <y1>"""
import sys
from PIL import Image

path, x0, y0, x1, y1 = sys.argv[1], *map(int, sys.argv[2:6])
img = Image.open(path).convert("RGB")
target = (0, 95, 184)
rows = {}
for y in range(y0, y1):
    for x in range(x0, x1):
        r, g, b = img.getpixel((x, y))
        if abs(r - target[0]) < 25 and abs(g - target[1]) < 25 and abs(b - target[2]) < 25:
            rows.setdefault(y, []).append(x)
# keep rows that look like a button face (long horizontal runs)
good = [(y, xs) for y, xs in rows.items() if len(xs) > 40]
if not good:
    sys.exit(1)
ys = [y for y, _ in good]
xs = [x for _, row in good for x in row]
print((min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2)
