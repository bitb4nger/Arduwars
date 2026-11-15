# Battle effect art for Arduwars FX: explosion (3 frames 16x16) and muzzle flash (2 frames 8x8).
# Black outline, white fill, transparent outside (fxdata-build makes the mask from alpha).
import math, sys
from PIL import Image, ImageDraw
out = sys.argv[1]
T, W, B = (0, 0, 0, 0), (255, 255, 255, 255), (0, 0, 0, 255)

def star(d, cx, cy, r1, r2, n, rot, fill, outline):
    pts = []
    for i in range(n * 2):
        r = r1 if i % 2 == 0 else r2
        a = rot + i * math.pi / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, fill=fill, outline=outline)

boom = Image.new('RGBA', (48, 16), T)
d = ImageDraw.Draw(boom)
# 0: flash of the hit
star(d, 7.5, 7.5, 5.5, 2.5, 6, 0.3, W, B); d.ellipse((6, 6, 9, 9), fill=B)
# 1: full burst
star(d, 16 + 7.5, 7.5, 7.5, 3.5, 8, 0.0, B, B); star(d, 16 + 7.5, 7.5, 5.5, 2.2, 8, 0.2, W, W)
d.ellipse((16 + 5, 5, 16 + 10, 10), fill=B); d.ellipse((16 + 6, 6, 16 + 9, 9), fill=W)
# 2: smoke puffs
for (x, y, r) in ((36, 9, 4), (41, 6, 4), (43, 10, 3), (38, 4, 2.5)):
    d.ellipse((x - r, y - r, x + r, y + r), fill=B)
for (x, y, r) in ((36, 9, 2.5), (41, 6, 2.5), (43, 10, 1.5), (38, 4, 1)):
    d.ellipse((x - r, y - r, x + r, y + r), fill=W)
boom.save(out + '/boom_16x16.png')

fl = Image.new('RGBA', (16, 8), T)
d = ImageDraw.Draw(fl)
star(d, 3.5, 3.5, 3.6, 1.4, 4, 0.0, W, B)
star(d, 8 + 3.5, 3.5, 3.6, 1.6, 5, 0.6, B, B); d.point((8 + 3, 3), fill=W); d.point((8 + 4, 4), fill=W)
fl.save(out + '/flash_8x8.png')
