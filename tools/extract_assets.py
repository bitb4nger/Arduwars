# One-off: turn the PROGMEM sprite arrays and maps of the original Arduwars
# into PNGs + an fxdata.txt, so the art lives on the FX chip.
import re, sys, os
from PIL import Image
SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
def arrays(fn):
    s = open(fn).read()
    s = re.sub(r'//[^\n]*', '', s)
    out = {}
    for m in re.finditer(r'(\w+)\s*\[\]\s*(?:PROGMEM)?\s*=\s*\{(.*?)\};', s, re.S):
        out[m.group(1)] = [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+|\d+', m.group(2))]
    return out
A = {}
for f in ['SpriteAssets.h', 'SpriteAnimations.h', 'Tileset.h', 'UnitSprites.h', 'MapData.h']:
    A.update(arrays(os.path.join(SRC, f)))
def topng(name, fname, masked):
    d = A[name]; w, h = d[0], d[1]; data = d[2:]
    pages = (h + 7) // 8; per = w * pages * (2 if masked else 1)
    n = len(data) // per
    img = Image.new('RGBA', (w * n, h), (0, 0, 0, 255))
    px = img.load()
    for f in range(n):
        fr = data[f * per:(f + 1) * per]
        for p in range(pages):
            for x in range(w):
                i = (p * w + x) * (2 if masked else 1)
                b = fr[i]; m = fr[i + 1] if masked else 0xFF
                for bit in range(8):
                    y = p * 8 + bit
                    if y >= h: continue
                    white = (b >> bit) & 1; opaque = (m >> bit) & 1
                    px[f * w + x, y] = (255, 255, 255, 255) if white else ((0, 0, 0, 255) if opaque else (0, 0, 0, 0))
    img.save(os.path.join(OUT, fname))
    print(fname, n, 'frames', w, 'x', h)
topng('worldSprite', 'worldTiles_16x16.png', False)
topng('unitsA_plus_mask', 'unitsA_16x16.png', True)
topng('unitsB_plus_mask', 'unitsB_16x16.png', True)
topng('gameCursorAnimation_plus_mask', 'cursor_32x32.png', True)
topng('selectionAnimation', 'selection_16x16.png', False)
topng('mapFOG_16x16', 'fog_16x16.png', False)
topng('mapMarkers_plus_mask', 'markers_8x8.png', True)
topng('arduwarsLogo', 'logo.png', False)
for k in ['mapData_1', 'mapData_2', 'mapData_3']:
    d = A[k]; print(k, d[0], 'x', d[1], len(d) - 10, 'tiles')
    with open(os.path.join(OUT, k + '.txt'), 'w') as f:
        f.write(','.join(map(str, d)))
