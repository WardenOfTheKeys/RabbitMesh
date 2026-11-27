#!/usr/bin/env python3
"""Holiday costumes as whole frames: from a costume atlas (art/costumes/holiday_atlas.png: one
column per holiday, a stride on the top row, the radio at his mouth on the bottom row) and the
base walk and talk frames (art/walk, art/talk), builds a full set of frames per holiday in
art/holiday/<name>/ (walk_0..7, talk_0..3, on the base frames' 443x443 canvas).

    python3 tools/make_holiday_frames.py [--atlas art/costumes/holiday_atlas.png]

Each frame is the costume's upper body (hat, head, coat, both arms, tail) from the atlas, scaled
onto the base frame's belt line, over that base frame's own pants and boots. So the stride, its
timing, the boot baseline and the planted foot are exactly the base walk's, and the firmware's
rig and foot data (rabbit_sprite.h) hold for every costume. The atlas's own pants are cut away
below its belt (pants are the dark magenta-purple; coat skirts, gloves and tails stay).

Which upper body goes on which frame: the walk frames and the first two talk frames (radio low)
take the stride's; the last two talk frames (radio at his mouth) take the talking one's.

Next to each frame it writes tags_<frame>.png (0 none, 1 visor, 2 radio screen, 4 mouth) so
tools/make_holiday_sprites.py knows which pixels glow, light up or open and close.
Needs Pillow + numpy.
"""
import argparse
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
NAMES = ["halloween", "thanksgiving", "christmas", "newyear", "july4"]
SCALE = 0.86            # atlas px -> base canvas px: the atlas head sits as high as his own ears
BASE_CUT = {"walk": 249, "talk": 251}      # first row of pants below the belt, base walk_0 / talk_0
ATLAS_BELT = {"walk": 266, "talk": 257}    # last row of the belt in an atlas cell (cropped to its figure)
TAIL_DEPTH = 40                            # his own tail hangs this far below the belt (base px)
GLOVE_DEPTH = 40                           # the back glove hangs this far below the belt (atlas px)
TAG_VISOR, TAG_LED, TAG_MOUTH = 1, 2, 4


def opaque(a):
    return a[:, :, 3] >= 128


def runs1d(v, min_len):
    out, s = [], None
    for i, x in enumerate(list(v) + [False]):
        if x and s is None:
            s = i
        if not x and s is not None:
            if i - s > min_len:
                out.append((s, i - 1))
            s = None
    return out


def cells(atlas):
    """{(name, 'walk'|'talk'): RGBA cell cropped to its figure}."""
    a = np.array(atlas)
    cols = runs1d(opaque(a).any(axis=0), 40)
    rows = runs1d(opaque(a).any(axis=1), 40)
    if len(cols) != len(NAMES) or len(rows) != 2:
        raise SystemExit(f"expected {len(NAMES)} columns and 2 rows of figures, found {len(cols)} and {len(rows)}")
    out = {}
    for n, (x0, x1) in zip(NAMES, cols):
        for r, (y0, y1) in zip(("walk", "talk"), rows):
            sub = a[y0:y1 + 1, x0:x1 + 1]
            ys, xs = np.where(opaque(sub))
            out[(n, r)] = sub[ys.min():ys.max() + 1, xs.min():xs.max() + 1].copy()
    return out


def blobs(m):
    """8-connected components of a mask, as lists of (y, x)."""
    h, w = m.shape
    seen = np.zeros_like(m)
    out = []
    for y0, x0 in zip(*np.where(m)):
        if seen[y0, x0]:
            continue
        st, pts = [(y0, x0)], []
        seen[y0, x0] = True
        while st:
            y, x = st.pop()
            pts.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and m[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        st.append((yy, xx))
        out.append(pts)
    return out


def upper_body(cell, belt):
    """The costume above the belt, and below it whatever isn't his pants or boots (coat skirts,
    the swinging glove, the tail): RGBA, same size as the cell."""
    a = cell.astype(int)
    r, g, b, al = a[:, :, 0], a[:, :, 1], a[:, :, 2], a[:, :, 3]
    op = al >= 128
    lum = (r + g + b) / 3
    pants = op & (lum > 22) & (lum < 120) & (abs(r - b) < 28) & (g < 0.6 * (r + b) / 2)
    dark = op & (lum <= 34)
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    neutral = op & (lum < 95) & (mx - mn < 13)                  # boots, gloves, outlines: black and grey
    keep = op.copy()
    below = np.zeros_like(op)
    below[belt + 1:, :] = True
    deep = np.zeros_like(op)
    deep[belt + GLOVE_DEPTH:, :] = True                          # below the hanging glove: boots and the shadow
    keep[below & pants] = False
    keep[deep & (neutral | dark)] = False
    # outlines below the belt stay only beside something kept that isn't outline (a coat's edge)
    solid = keep & ~dark
    near = np.zeros_like(solid)
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            near |= np.roll(np.roll(solid, dy, 0), dx, 1)
    keep[below & dark & ~near] = False
    # only what hangs from above the belt (boots and stray pants shading go)
    lab = np.zeros(op.shape, bool)
    for pts in blobs(keep):
        if min(p[0] for p in pts) <= belt:
            for y, x in pts:
                lab[y, x] = True
    out = cell.copy()
    out[~lab] = 0
    return out


def tags_of(cell, belt):
    """Tag map for a cell: the visor (the widest bright patch, green or gold, in the head), the
    radio's screen (the bright green patch nearest the glove holding it), the mouth (red, in the
    face under the visor)."""
    a = cell.astype(int)
    r, g, b = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    op = opaque(cell)
    h, w = op.shape
    t = np.zeros((h, w), np.uint8)
    bright_green = op & (g > 150) & (g - r > 40)
    bright_gold = op & (r > 170) & (g > 140) & (b < 120) & (r - b > 90)
    head = np.zeros_like(op)
    head[: belt * 6 // 10, :] = True
    best, bw = None, -1
    for pts in blobs((bright_green | bright_gold) & head):
        xs = [p[1] for p in pts]
        ys = [p[0] for p in pts]
        wdt = max(xs) - min(xs) + 1
        if wdt > bw and max(ys) - min(ys) < 14 and len(pts) > 20:
            best, bw = pts, wdt
    vx0 = vx1 = vy = None
    if best:
        ys = [p[0] for p in best]
        xs = [p[1] for p in best]
        vx0, vx1, vy = min(xs), max(xs), (min(ys) + max(ys)) // 2
        y0, y1 = min(ys), max(ys)
        reg = (bright_green | bright_gold)
        t[y0:y1 + 1, min(xs):max(xs) + 1][reg[y0:y1 + 1, min(xs):max(xs) + 1]] = TAG_VISOR
    # the radio's screen: a bright green patch below the visor, in front of the face
    if vy is not None:
        cand = []
        for pts in blobs(bright_green):
            ys = [p[0] for p in pts]
            xs = [p[1] for p in pts]
            if min(ys) > vy + 12 and min(xs) > vx0 and len(pts) >= 12:
                cand.append(pts)
        if cand:
            for y, x in max(cand, key=len):
                t[y, x] = TAG_LED
        # the mouth: red in the face, below the visor, in front of its middle
        face = np.zeros_like(op)
        face[vy + 8: vy + 50, (vx0 + vx1) // 2: vx1 + 20] = True
        red = op & (r > 100) & (g < 110) & (b < 120) & (r - g > 45) & face
        t[red & (t == 0)] = TAG_MOUTH
    return t


def place(dst, src, ox, oy, mask=None):
    """Paint src's opaque pixels into dst at (ox, oy)."""
    h, w = src.shape[:2]
    H, W = dst.shape[:2]
    for y in range(h):
        yy = oy + y
        if not 0 <= yy < H:
            continue
        for x in range(w):
            xx = ox + x
            if 0 <= xx < W and (mask[y, x] if mask is not None else src[y, x, 3] >= 128):
                dst[yy, xx] = src[y, x]


def shrink(a, f, nearest=False):
    h, w = a.shape[:2]
    size = (max(1, round(w * f)), max(1, round(h * f)))
    if nearest:
        return np.array(Image.fromarray(a).resize(size, Image.NEAREST))
    p = a.astype(np.float32)
    p[:, :, :3] *= p[:, :, 3:4] / 255.0
    s = np.array(Image.fromarray(p.astype(np.uint8), "RGBA").resize(size, Image.LANCZOS)).astype(np.float32)
    al = s[:, :, 3:4]
    rgb = np.where(al > 0, s[:, :, :3] / np.maximum(al, 1) * 255.0, 0)
    out = np.concatenate([np.clip(rgb, 0, 255), al], axis=2).astype(np.uint8)
    out[out[:, :, 3] < 128] = 0
    out[out[:, :, 3] >= 128, 3] = 255
    return out


def offset(ref, cur, rows):
    """How far cur's upper body sits from ref's (dx, dy), by matching their outlines."""
    y0, y1 = rows
    best, bs = (0, 0), -1
    A = opaque(ref)[y0:y1]
    for dy in range(-4, 5):
        for dx in range(-8, 9):
            B = np.roll(np.roll(opaque(cur), -dy, 0), -dx, 1)[y0:y1]
            s = (A & B).sum() - (A ^ B).sum()
            if s > bs:
                bs, best = s, (dx, dy)
    return best


def front(a, y):
    xs = np.where(opaque(a)[y])[0]
    return int(xs.max())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--atlas", default=os.path.join(ROOT, "art", "costumes", "holiday_atlas.png"))
    ap.add_argument("--out", default=os.path.join(ROOT, "art", "holiday"))
    args = ap.parse_args()
    C = cells(Image.open(args.atlas).convert("RGBA"))
    base = {"walk": [np.array(Image.open(os.path.join(ROOT, "art", "walk", f"walk_{i}.png")).convert("RGBA")) for i in range(8)],
            "talk": [np.array(Image.open(os.path.join(ROOT, "art", "talk", f"talk_{i}.png")).convert("RGBA")) for i in range(4)]}
    # each base frame: its belt line and its front at the belt, from frame 0 of its kind and how
    # far its upper body sits from that one's
    anchors = {}
    for kind, frames in base.items():
        cut0 = BASE_CUT[kind]
        f0 = front(frames[0], cut0 - 4)
        for i, fr in enumerate(frames):
            dx, dy = offset(frames[0], fr, (40, cut0 - 60)) if i else (0, 0)
            anchors[(kind, i)] = (cut0 + dy, f0 + dx)
    for n in NAMES:
        d = os.path.join(args.out, n)
        os.makedirs(d, exist_ok=True)
        ups = {}
        for r in ("walk", "talk"):
            cell = C[(n, r)]
            belt = ATLAS_BELT[r]
            up = upper_body(cell, belt)
            tg = tags_of(cell, belt)
            tg[up[:, :, 3] < 128] = 0
            fr_x = front(cell, belt - 4)
            ups[r] = (shrink(up, SCALE), shrink(np.dstack([tg, tg, tg, np.where(tg > 0, 255, 0).astype(np.uint8)]), SCALE, True),
                      round(belt * SCALE), round(fr_x * SCALE))
        for kind, frames in base.items():
            for i, fr in enumerate(frames):
                use = "talk" if (kind == "talk" and i >= 2) else "walk"
                up, tg, belt_s, front_s = ups[use]
                cut, fx = anchors[(kind, i)]
                out = np.zeros_like(fr)
                legs = fr.copy()
                legs[:cut] = 0
                lum = legs[:, :, :3].astype(int).mean(axis=2)
                hip = np.zeros(lum.shape, bool)
                hip[cut:cut + TAIL_DEPTH, :fx - 60] = True              # his own tail, at the back of his hip:
                legs[(lum > 170) & hip] = 0                             # the costume brings one
                soft = legs[:, :, 3] > 0                                 # soft edges too: the shrink averages them in
                out[soft] = legs[soft]
                ox, oy = fx - front_s, cut - 1 - belt_s
                place(out, up, ox, oy)
                tmap = np.zeros(fr.shape[:2], np.uint8)
                tl = np.zeros_like(fr)
                place(tl, tg, ox, oy, mask=tg[:, :, 3] > 0)
                tmap[tl[:, :, 3] > 0] = tl[tl[:, :, 3] > 0][:, 0]
                name = f"{kind}_{i}"
                Image.fromarray(out, "RGBA").save(os.path.join(d, name + ".png"))
                Image.fromarray(tmap, "L").save(os.path.join(d, "tags_" + name + ".png"))
        print(f"{n}: 12 frames in {d}")


if __name__ == "__main__":
    main()
