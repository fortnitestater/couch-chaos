# Builds models/cards.png: the 52 card faces and a card back, printed on the cards in 3D (table.h CardAt).
# Layout: 2048 x 1024. Row = suit (spades, hearts, diamonds, clubs), column = rank (A..K); row 4, column 0 = the back pattern
# (light grey on white, tinted by the back colour). Each card is 143 x 204 px at x = col * 157 + 7, y = row * 204.
#   python native/tools/make_cards.py
import math, os
from PIL import Image, ImageDraw, ImageFont

S = 2                               # drawn at twice the size, then scaled down (smooth edges)
CW, CH, PITCH = 143, 204, 157
W, H = 2048, 1024
RED, BLACK = (214, 40, 57), (26, 29, 41)
PAPER = (253, 252, 247)
FONT = 'C:/Windows/Fonts/georgiab.ttf'
here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, '..', '..', 'models', 'cards.png')

def heart_poly(n=120):
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        pts.append((x, y))
    return pts

def suit_img(s, col, px=400):
    """one suit symbol, centred in a px x px transparent square, pointing up"""
    im = Image.new('RGBA', (px, px), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    c = px / 2
    def fit(pts, w, h, cx, cy, flip=False):
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        k = min(w / (x1 - x0), h / (y1 - y0))
        return [(cx + (x - (x0 + x1) / 2) * k, cy + ((-(y - (y0 + y1) / 2)) if flip else (y - (y0 + y1) / 2)) * k) for x, y in pts]
    if s == 1:      # heart
        d.polygon(fit(heart_poly(), px * 0.9, px * 0.84, c, c), fill=col)
    elif s == 2:    # diamond: slightly curved sides
        V = [(0, -1), (0.72, 0), (0, 1), (-0.72, 0)]; pts = []
        for i in range(4):
            (ax, ay), (bx, by) = V[i], V[(i + 1) % 4]
            for k in range(16):
                u = k / 16; x, y = ax + (bx - ax) * u, ay + (by - ay) * u; pull = 1 - 0.07 * math.sin(u * math.pi)
                pts.append((x * pull, y * pull))
        d.polygon([(c + x * px * 0.47, c + y * px * 0.47) for x, y in pts], fill=col)
    elif s == 0:    # spade: an upside-down heart and a flared stem
        d.polygon(fit(heart_poly(), px * 0.86, px * 0.72, c, c - px * 0.08, flip=True), fill=col)
        d.polygon([(c, c + px * 0.05), (c - px * 0.17, c + px * 0.45), (c + px * 0.17, c + px * 0.45)], fill=col)
    else:           # club: three balls and a stem
        r = px * 0.2
        for (x, y) in [(c, c - px * 0.2), (c - px * 0.215, c + px * 0.1), (c + px * 0.215, c + px * 0.1)]:
            d.ellipse([x - r, y - r, x + r, y + r], fill=col)
        d.ellipse([c - r * 0.6, c - r * 0.5, c + r * 0.6, c + r * 0.7], fill=col)
        d.polygon([(c, c + px * 0.02), (c - px * 0.15, c + px * 0.46), (c + px * 0.15, c + px * 0.46)], fill=col)
    return im

def paste_suit(im, s, cx, cy, size, col, down=False):
    g = suit_img(s, col).resize((int(size), int(size)), Image.LANCZOS)
    if down: g = g.rotate(180)
    im.alpha_composite(g, (int(cx - size / 2), int(cy - size / 2)))

def rounded(d, box, r, fill, outline=None, width=1):
    d.rounded_rectangle(box, r, fill=fill, outline=outline, width=width)

# where the pips go (column -1/0/1, row 0..1 down the card)
PIPS = {
    2: [(0, 0), (0, 1)], 3: [(0, 0), (0, .5), (0, 1)],
    4: [(-1, 0), (1, 0), (-1, 1), (1, 1)], 5: [(-1, 0), (1, 0), (0, .5), (-1, 1), (1, 1)],
    6: [(-1, 0), (1, 0), (-1, .5), (1, .5), (-1, 1), (1, 1)],
    7: [(-1, 0), (1, 0), (0, .25), (-1, .5), (1, .5), (-1, 1), (1, 1)],
    8: [(-1, 0), (1, 0), (0, .25), (-1, .5), (1, .5), (0, .75), (-1, 1), (1, 1)],
    9: [(-1, 0), (1, 0), (-1, 1 / 3), (1, 1 / 3), (0, .5), (-1, 2 / 3), (1, 2 / 3), (-1, 1), (1, 1)],
    10: [(-1, 0), (1, 0), (0, 1 / 6), (-1, 1 / 3), (1, 1 / 3), (-1, 2 / 3), (1, 2 / 3), (0, 5 / 6), (-1, 1), (1, 1)],
}
RANKS = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K']

def crown(d, cx, cy, w, kind, gold, dark):
    """a simple crown (K), tiara (Q) or cap with a feather (J)"""
    if kind == 'K':
        pts = [(cx - w / 2, cy + w * 0.28), (cx - w / 2, cy - w * 0.1), (cx - w * 0.3, cy + w * 0.06), (cx - w * 0.15, cy - w * 0.26), (cx, cy + w * 0.02),
               (cx + w * 0.15, cy - w * 0.26), (cx + w * 0.3, cy + w * 0.06), (cx + w / 2, cy - w * 0.1), (cx + w / 2, cy + w * 0.28)]
        d.polygon(pts, fill=gold, outline=dark, width=2 * S)
        for x in (-0.5, -0.15, 0.15, 0.5): d.ellipse([cx + x * w - 5 * S, cy - w * (0.1 if abs(x) == 0.5 else 0.26) - 5 * S - 4 * S, cx + x * w + 5 * S, cy - w * (0.1 if abs(x) == 0.5 else 0.26) + 5 * S - 4 * S], fill=gold, outline=dark, width=S)
    elif kind == 'Q':
        d.chord([cx - w / 2, cy - w * 0.32, cx + w / 2, cy + w * 0.5], 180, 360, fill=gold, outline=dark, width=2 * S)
        d.rectangle([cx - w / 2, cy + w * 0.09, cx + w / 2, cy + w * 0.26], fill=gold, outline=dark, width=2 * S)
        d.ellipse([cx - 7 * S, cy - w * 0.2, cx + 7 * S, cy - w * 0.2 + 14 * S], fill=(200, 40, 60), outline=dark, width=S)
    else:
        d.rounded_rectangle([cx - w * 0.42, cy - w * 0.12, cx + w * 0.42, cy + w * 0.26], 8 * S, fill=gold, outline=dark, width=2 * S)
        d.polygon([(cx + w * 0.2, cy - w * 0.1), (cx + w * 0.55, cy - w * 0.42), (cx + w * 0.42, cy - w * 0.02)], fill=(200, 40, 60), outline=dark)

def face(r, s):
    w, h = CW * S, CH * S
    im = Image.new('RGBA', (w, h), PAPER + (0,)); d = ImageDraw.Draw(im)   # outside the rounded corners: see-through (paper colour, so no dark fringe)
    rounded(d, [1 * S, 1 * S, w - 1 - 1 * S, h - 1 - 1 * S], 11 * S, PAPER + (255,), (205, 200, 190), 2 * S)
    col = RED if s in (1, 2) else BLACK
    rank = RANKS[r]
    f = ImageFont.truetype(FONT, int((27 if rank == '10' else 31) * S))
    def index(img):
        dd = ImageDraw.Draw(img)
        dd.text((18 * S, 22 * S), rank, font=f, fill=col, anchor='mm')
        paste_suit(img, s, 18 * S, 46 * S, 18 * S, col)
    index(im)
    top = im.rotate(180); index(top); im = top.rotate(180)   # the same index in the opposite corner, upside down
    d = ImageDraw.Draw(im)
    if r == 0:      # ace: one big pip
        paste_suit(im, s, w / 2, h / 2, (78 if s == 0 else 62) * S, col)
    elif r >= 10:   # court cards: a framed portrait split in two mirrored halves
        kind = rank
        frame = [31 * S, 27 * S, w - 31 * S, h - 27 * S]
        fill = {'K': (247, 230, 190), 'Q': (245, 224, 228), 'J': (226, 234, 247)}[kind]
        edge = {'K': (196, 146, 40), 'Q': (198, 60, 90), 'J': (60, 100, 180)}[kind]
        d.rectangle(frame, fill=fill, outline=edge, width=3 * S)
        half = Image.new('RGBA', (frame[2] - frame[0], (frame[3] - frame[1]) // 2), (0, 0, 0, 0)); hd = ImageDraw.Draw(half)
        hw, hh = half.size
        crown(hd, hw / 2, hh * 0.26, hw * 0.52, kind, (240, 192, 64), (110, 70, 20))
        big = ImageFont.truetype(FONT, int(46 * S))
        hd.text((hw / 2, hh * 0.7), rank, font=big, fill=col, anchor='mm')
        paste_suit(half, s, hw * 0.2, hh * 0.72, 18 * S, col)
        paste_suit(half, s, hw * 0.8, hh * 0.72, 18 * S, col)
        im.alpha_composite(half, (frame[0], frame[1]))
        im.alpha_composite(half.rotate(180), (frame[0], frame[1] + hh + 1))
        d = ImageDraw.Draw(im)
        d.line([frame[0] + 4 * S, (frame[1] + frame[3]) / 2, frame[2] - 4 * S, (frame[1] + frame[3]) / 2], fill=edge, width=2 * S)
    else:
        n = r + 1; x0, x1, y0, y1 = 46 * S, w - 46 * S, 40 * S, h - 40 * S
        for cx, cy in PIPS[n]:
            px = (x0 + x1) / 2 + cx * (x1 - x0) / 2; py = y0 + cy * (y1 - y0)
            paste_suit(im, s, px, py, 27 * S, col, down=cy > 0.5)
    return im

def back():
    """grey-on-white so a tint colours it: a fine lattice and an oval medallion"""
    w, h = CW * S, CH * S
    im = Image.new('RGBA', (w, h), (255, 255, 255, 255)); d = ImageDraw.Draw(im)
    step = 12 * S
    for k in range(-h, w + h, step):
        d.line([(k, 0), (k + h, h)], fill=(205, 205, 205), width=2 * S)
        d.line([(k, h), (k + h, 0)], fill=(205, 205, 205), width=2 * S)
    d.rectangle([4 * S, 4 * S, w - 4 * S, h - 4 * S], outline=(170, 170, 170), width=3 * S)
    d.ellipse([w * 0.22, h * 0.3, w * 0.78, h * 0.7], fill=(240, 240, 240), outline=(160, 160, 160), width=3 * S)
    d.ellipse([w * 0.3, h * 0.36, w * 0.7, h * 0.64], outline=(185, 185, 185), width=2 * S)
    f = ImageFont.truetype(FONT, int(30 * S))
    d.text((w / 2, h / 2), 'CC', font=f, fill=(150, 150, 150), anchor='mm')
    return im

atlas = Image.new('RGBA', (W * S, H * S), PAPER + (0,))
for s in range(4):
    for r in range(13):
        atlas.alpha_composite(face(r, s), ((r * PITCH + 7) * S, s * CH * S))
atlas.alpha_composite(back(), (7 * S, 4 * CH * S))
atlas = atlas.resize((W, H), Image.LANCZOS)
atlas.save(out, optimize=True)
print('wrote', os.path.normpath(out), os.path.getsize(out), 'bytes')
