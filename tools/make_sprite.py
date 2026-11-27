#!/usr/bin/env python3
"""Turns the character art into src/rabbit_sprite.h (+ .cpp, the pixels): palette sprite
frames the firmware draws itself.

    python3 tools/make_sprite.py [--art art/rabbit.png] [--walk art/walk] [--out src/rabbit_sprite.h]
                                 [--height 122] [--rig HIP,SPLIT,EAR,AX0,AY0,AX1,AY1]

Input: the standing art (any size, transparent background), and, if art/walk holds
walk_0.png ... walk_N.png (the same canvas size, feet on the same ground line), those as
the walk's frames: tools/make_walk.py builds them from the standing art, or they can be
hand-drawn. With no walk frames there is one frame and the firmware bends it instead.

Every frame is cropped to one shared box round all of them, scaled down by area
averaging with premultiplied alpha (so edges don't pick up a dark fringe), cut to
opaque/transparent at half alpha, and reduced to ONE palette shared by all the frames
(at most 61 colours, so nothing changes colour between frames; index 0 is transparent).
Three more entries are tagged so the firmware can animate them: the visor's cyan (it
glows, blinks, turns amber with unread messages), the radio screen's orange (lit while
he talks) and a 1-pixel rim round the figure (lit in the theme's colour).

It also finds, on the standing art, the joints the firmware moves like a cut-out
puppet: the hip (where the legs part, and the column between them), the base of the
ears, and the box of the arm holding the radio (round its screen, pivoting at the
elbow). Check them in the header if new art moves oddly, and override with --rig.
Needs Pillow.
"""
import argparse
import colorsys
import glob
import os
import re

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TAG_NONE, TAG_VISOR, TAG_LED, TAG_RIM, TAG_MOUTH = 0, 1, 2, 3, 4
COLOURS = 61


def tag(rgb):
    """What a full-size pixel is: the visor's cyan, the radio's orange, or neither."""
    h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
    hd = h * 360
    if 150 <= hd <= 200 and s > 0.35 and v > 0.5:
        return TAG_VISOR
    if 10 <= hd <= 55 and s > 0.5 and v > 0.6:
        return TAG_LED
    r, g, b = rgb
    if r > 110 and g < 100 and b < 100 and r - b > 60:     # dark red: not the magenta rim light
        return TAG_MOUTH
    return TAG_NONE


def mask(im, which):
    """Full-size mask of one tag, so small glowing parts survive the scaling."""
    m = Image.new("L", im.size)
    sp, mp = im.load(), m.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = sp[x, y]
            if a >= 128 and tag((r, g, b)) == which:
                mp[x, y] = 255
    return m


def brightest(im, m):
    best, bv = None, -1
    sp, mp = im.load(), m.load()
    for y in range(0, im.height, 2):
        for x in range(0, im.width, 2):
            if mp[x, y]:
                r, g, b, _ = sp[x, y]
                if r + g + b > bv:
                    bv, best = r + g + b, (r, g, b)
    return best


def seal(d, W, H, max_w):
    """Pinholes and cracks: a see-through pocket walled in by his body that is a few pixels
    (up to 5) or no more than max_w pixels wide in any row (a seam between two legs standing
    close, a stray pixel at the waist) is one the city would show through. Sealed with the
    colour beside it. Bigger pockets are air the artist drew (between the radio and his
    mouth) and stay. Returns (pixels sealed, sizes of the pockets kept)."""
    reach = [False] * (W * H)
    st = [i for i in range(W * H) if (i % W in (0, W - 1) or i // W in (0, H - 1)) and not d[i]]
    for i in st:
        reach[i] = True
    while st:
        i = st.pop()
        x, y = i % W, i // W
        for xx, yy in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= xx < W and 0 <= yy < H and not d[yy * W + xx] and not reach[yy * W + xx]:
                reach[yy * W + xx] = True
                st.append(yy * W + xx)
    seen, sealed, kept = [False] * (W * H), 0, []
    for i in range(W * H):
        if d[i] or reach[i] or seen[i]:
            continue
        pocket, st = [], [i]
        seen[i] = True
        while st:
            j = st.pop()
            pocket.append(j)
            x, y = j % W, j // W
            for xx, yy in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= xx < W and 0 <= yy < H and not d[yy * W + xx] and not seen[yy * W + xx]:
                    seen[yy * W + xx] = True
                    st.append(yy * W + xx)
        rows = {}
        for j in pocket:
            rows.setdefault(j // W, []).append(j % W)
        widest = max(max(xs) - min(xs) + 1 for xs in rows.values())
        if len(pocket) <= 5 or widest <= max_w:
            todo = list(pocket)
            while todo:                               # fill from the edges in
                left = []
                for j in todo:
                    x, y = j % W, j // W
                    for xx, yy in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                        if 0 <= xx < W and 0 <= yy < H and d[yy * W + xx]:
                            d[j] = d[yy * W + xx]
                            break
                    else:
                        left.append(j)
                if len(left) == len(todo):
                    break
                todo = left
            sealed += len(pocket)
        else:
            kept.append(len(pocket))
    return sealed, kept


class Frame:
    """One picture, scaled: pixels as RGBA, opacity, and how much of each pixel was
    visor or LED at full size."""

    def __init__(self, im, box, w, h):
        self.full = im.crop(box)
        small = self.full.convert("RGBa").resize((w, h), Image.BOX).convert("RGBA")
        self.px = small.load()
        self.w, self.h = w, h
        self.opaque = [[self.px[x, y][3] >= 128 for x in range(w)] for y in range(h)]
        self.cover = {t: mask(self.full, t).resize((w, h), Image.BOX).load() for t in (TAG_VISOR, TAG_LED, TAG_MOUTH)}


def runs(opaque, y, w):
    r, start = [], None
    for x in range(w + 1):
        o = x < w and opaque[y][x]
        if o and start is None:
            start = x
        if not o and start is not None:
            r.append((start, x - 1))
            start = None
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--art", default=os.path.join(HERE, "..", "art", "rabbit.png"))
    ap.add_argument("--walk", default=os.path.join(HERE, "..", "art", "walk"))
    ap.add_argument("--talk", default=os.path.join(HERE, "..", "art", "talk"))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "src", "rabbit_sprite.h"))
    ap.add_argument("--height", type=int, default=122)
    ap.add_argument("--rig", default=None)
    a = ap.parse_args()

    stand = Image.open(a.art).convert("RGBA")
    paths = sorted(glob.glob(os.path.join(a.walk, "walk_*.png")),
                   key=lambda p: int(re.search(r"(\d+)\.png$", p).group(1)))
    walk = [Image.open(p).convert("RGBA") for p in paths]
    tpaths = sorted(glob.glob(os.path.join(a.talk, "talk_*.png")),
                    key=lambda p: int(re.search(r"(\d+)\.png$", p).group(1)))
    talk = [Image.open(p).convert("RGBA") for p in tpaths] if walk else []
    for p, im in zip(paths + tpaths, walk + talk):
        if im.size != stand.size:
            raise SystemExit(f"{p} is {im.size}, the standing art is {stand.size}: walk frames need the same canvas")

    # One crop box round everything, so the frames line up.
    def bbox(im):
        return im.getchannel("A").point(lambda v: 255 if v > 32 else 0).getbbox()
    boxes = [bbox(im) for im in [stand] + walk + talk]
    box = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))
    H0 = a.height
    W0 = max(1, round((box[2] - box[0]) * H0 / (box[3] - box[1])))

    ref = Frame(stand, box, W0, H0)                       # the rig and the metrics come from this
    frames = [Frame(im, box, W0, H0) for im in walk + talk] or [ref]

    # One palette for all of them: quantise the frames side by side.
    sheet = Image.new("RGB", (W0 * len(frames), H0))
    sp = sheet.load()
    for i, f in enumerate(frames):
        for y in range(H0):
            for x in range(W0):
                sp[i * W0 + x, y] = f.px[x, y][:3] if f.opaque[y][x] else (0, 0, 0)
    q = sheet.quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal, qp = q.getpalette()[: COLOURS * 3], q.load()
    used = sorted({qp[i * W0 + x, y] for i, f in enumerate(frames) for y in range(H0) for x in range(W0) if f.opaque[y][x]})
    remap = {old: n + 1 for n, old in enumerate(used)}
    palette = [tuple(pal[o * 3: o * 3 + 3]) for o in used]
    tags = [TAG_NONE] * len(palette)
    glow_idx = {}
    for t in (TAG_VISOR, TAG_LED):
        c = brightest(stand, mask(stand, t))
        if c:
            palette.append(c)
            tags.append(t)
            glow_idx[t] = len(palette)
    # The mouth (only in the talk frames: it opens and closes as he talks). Its own entry,
    # and the fur colour it closes to (the palette's brightest, nearest white).
    fur_idx = 0
    if talk:
        mc = [brightest(im, mask(im, TAG_MOUTH)) for im in talk]
        mc = [c for c in mc if c]
        if mc:
            palette.append(mc[0])
            tags.append(TAG_MOUTH)
            glow_idx[TAG_MOUTH] = len(palette)
        fur_idx = 1 + max(range(len(used)), key=lambda n: sum(palette[n]))
    palette.append((0, 0, 0))
    tags.append(TAG_RIM)
    rim = len(palette)

    # Each frame's pixels: palette indices, the glow painted in, then a rim, in a canvas
    # one pixel bigger each way.
    W, H = W0 + 2, H0 + 2
    datas = []
    for i, f in enumerate(frames):
        i_frame = i
        d = [0] * (W * H)
        for y in range(H0):
            for x in range(W0):
                if f.opaque[y][x]:
                    v = remap[qp[i * W0 + x, y]]
                    for t, idx in glow_idx.items():
                        if f.cover[t][x, y] >= (30 if t == TAG_MOUTH else 85):   # the mouth is a pixel or two
                            v = idx
                    d[(y + 1) * W + x + 1] = v
        sealed1, kept = seal(d, W, H, max_w=3)       # cracks a few pixels wide, before the rim
        # Shrinking can round a boot's sole up a pixel: put the lowest row back on the
        # ground (the row above the rim's), so his feet never float.
        low = max((y for y in range(H) if any(d[y * W:(y + 1) * W])), default=H - 2)
        if 0 < H - 2 - low and walk:
            k = H - 2 - low
            d = [0] * (W * k) + d[: W * (H - k)]
        out = list(d)
        for y in range(H):
            for x in range(W):
                if d[y * W + x]:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < W and 0 <= yy < H and d[yy * W + xx]:
                        out[y * W + x] = rim
                        break
        sealed2, kept = seal(out, W, H, max_w=1)    # and what the rim leaves of a pocket
        if sealed1 or sealed2 or kept:
            print(f"frame {i_frame}: sealed {sealed1 + sealed2} px of cracks and pinholes, kept air pockets of {kept} px")
        datas.append(out)

    # Metrics and the rig, from the standing art.
    sd = [0] * (W * H)
    for y in range(H0):
        for x in range(W0):
            if ref.opaque[y][x]:
                sd[(y + 1) * W + x + 1] = 1
    opq = [[sd[y * W + x] != 0 for x in range(W)] for y in range(H)]

    def centre(t):
        pts = [(x + 1, y + 1) for y in range(H0) for x in range(W0) if ref.opaque[y][x] and ref.cover[t][x, y] >= 85]
        return (sum(p[0] for p in pts) // len(pts), sum(p[1] for p in pts) // len(pts)) if pts else None
    vx, vy = centre(TAG_VISOR) or (W // 2, H // 4)
    lx, ly = centre(TAG_LED) or (W - 4, H // 3)
    widths = [sum(opq[y]) for y in range(H)]
    waist = max(range(int(H * 0.45), int(H * 0.70)), key=lambda y: widths[y])
    feet = [x for x in range(W) if opq[H - 2][x]] or [W // 2]
    footx = (feet[0] + feet[-1]) // 2
    hip, split = H - 1, W // 2
    for y in range(H - 3, H // 2, -1):
        r = runs(opq, y, W)
        if len(r) >= 2 and r[1][0] - r[0][1] > 1:
            hip, split = y, (r[0][1] + r[1][0]) // 2
        elif hip < H - 1 and len(r) < 2:
            break
    hip -= 2
    ear, ear_split = H // 6, vx
    for y in range(1, max(2, vy - 6)):          # above the visor: lower down an arm held out is a run too
        r = runs(opq, y, W)
        if len(r) >= 2:
            ear, ear_split = y, (r[0][1] + r[1][0]) // 2   # the gap between the two ears

    ax0, ay0, ax1, ay1 = max(0, lx - 4), max(0, ly - 12), W - 1, min(H - 1, ly + 20)
    if a.rig:
        hip, split, ear, ax0, ay0, ax1, ay1 = [int(v) for v in a.rig.split(",")]

    # The planted foot. In each walk drawing, the boot on the ground that is still on the
    # ground in the next drawing is the one carrying his weight; how far it moved back
    # between the two is how far the firmware slides it, a little every screen frame, so
    # it never stands still while the street goes by. Its sum over the stride is how far
    # the street goes by in one stride.
    nwalk = len(walk)
    stance = []
    def ground_runs(d):
        y = H - 3                                  # the row above the soles' rim
        row = [d[y * W + x] != 0 and d[y * W + x] != rim for x in range(W)]
        out, st0 = [], None
        for x in range(W + 1):
            v = x < W and row[x]
            if v and st0 is None:
                st0 = x
            if not v and st0 is not None:
                out.append((st0, x - 1))
                st0 = None
        return [r for r in out if r[1] - r[0] >= 2]
    for k in range(nwalk):
        cur, nxt = ground_runs(datas[k]), ground_runs(datas[(k + 1) % nwalk])
        # The sole's back edge from this drawing to the next: the foot carrying him moves
        # back (up to 20 px between drawings: the landing heel moves furthest). Of the boots
        # on the ground, the one that moves back the most is his weight; a boot that stays
        # put is the one just landing.
        best = None
        for r in cur:
            for q in nxt:
                d = q[0] - r[0]
                if -20 <= d <= 2 and (best is None or d < best[2]):
                    best = (max(0, r[0] - 4), min(W - 1, r[1] + 4), d)   # the whole boot, not just the sole
        stance.append(best or (0, -1, 0))
    # A drawing whose planted boot has no match in the next one (the artist lifted that heel at
    # the next contact, so only the landing boot is on the ground there): its boot is still the
    # one on the ground now, and it moves back at about its neighbours' pace. Without this the
    # street would stop dead for that drawing.
    for k in range(nwalk):
        if stance[k][1] >= stance[k][0]:
            continue
        gr = ground_runs(datas[k])
        if not gr:
            continue
        near = [stance[(k + j) % nwalk][2] for j in (-1, 1) if stance[(k + j) % nwalk][1] >= stance[(k + j) % nwalk][0]]
        if not near:
            continue
        r = max(gr, key=lambda r: r[1] - r[0])
        stance[k] = (max(0, r[0] - 4), min(W - 1, r[1] + 4), round(sum(near) / len(near)))
        print(f"walk drawing {k}: planted boot not found in the next drawing; using its own, stepping {stance[k][2]} px")
    stride_px = -sum(s_[2] for s_ in stance)
    # The trailing boot: at a contact both feet are down; the back one stays planted until
    # it lifts, so it moves with the ground too (the firmware moves it at the ground's pace).
    trail = []
    for k in range(nwalk):
        st_ = stance[k]
        others = [r for r in ground_runs(datas[k]) if r[1] < st_[0] or r[0] > st_[1]]
        if others:
            r = max(others, key=lambda r: r[1] - r[0])
            trail.append((max(0, r[0] - 4), min(W - 1, r[1] + 4)))
        else:
            trail.append((0, -1))
    knee = hip + (H - hip) * 45 // 100

    def c565(c):
        r, g, b = c
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

    n = len(datas)
    L = []
    L.append("// Generated by tools/make_sprite.py from art/rabbit.png" + (" and art/walk" if walk else "") + " - edit the art, not this.")
    L.append("// The Neon Rabbit theme's character as palette sprite frames: index 0 is transparent.")
    L.append("#pragma once")
    L.append("#include <stdint.h>")
    L.append("")
    L.append("namespace rabbitsprite {")
    L.append(f"constexpr int W = {W}, H = {H};")
    L.append(f"constexpr int NFRAMES = {n};          // {'the walk, then the talk frames' if walk else 'one pose: the firmware bends it to walk'}")
    L.append(f"constexpr bool WALK_FRAMES = {'true' if walk else 'false'};")
    L.append(f"constexpr int NWALK = {nwalk if walk else 1};            // frames 0.. : one stride")
    L.append(f"constexpr int NTALK = {len(talk)};            // frames NWALK.. : radio low -> at his mouth")
    L.append(f"constexpr int FUR_IDX = {fur_idx};          // the colour his mouth closes to")
    L.append("// The planted foot: per walk drawing, the columns of the boot carrying his weight and")
    L.append("// how far (px) it moves back by the next drawing; the knee row it bends from; and how")
    L.append("// far the ground goes by in one stride, so his foot and the street move together.")
    st_ = stance if walk else [(0, -1, 0)]
    L.append("static const int8_t STANCE_X0[NWALK] = {" + ", ".join(str(v[0]) for v in st_) + "};")
    L.append("static const int8_t STANCE_X1[NWALK] = {" + ", ".join(str(v[1]) for v in st_) + "};")
    L.append("static const int8_t STANCE_DX[NWALK] = {" + ", ".join(str(v[2]) for v in st_) + "};")
    tr_ = trail if walk else [(0, -1)]
    L.append("// The trailing boot at a contact (both feet down): it moves with the ground until it lifts.")
    L.append("static const int8_t TRAIL_X0[NWALK] = {" + ", ".join(str(v[0]) for v in tr_) + "};")
    L.append("static const int8_t TRAIL_X1[NWALK] = {" + ", ".join(str(v[1]) for v in tr_) + "};")
    L.append(f"constexpr int KNEE_Y = {knee}, STRIDE_PX = {stride_px if walk else 0};")
    L.append(f"constexpr int VISOR_X = {vx}, VISOR_Y = {vy};   // the visor's centre")
    L.append(f"constexpr int LED_X = {lx}, LED_Y = {ly};       // the radio's screen")
    L.append(f"constexpr int WAIST_Y = {waist};               // rows above this breathe")
    L.append(f"constexpr int FOOT_X = {footx};                // the middle of his feet")
    L.append("// The puppet rig (rabbit.h): hip row and the column between the legs, the row the")
    L.append("// ears grow from, and the radio arm's box (it pivots at its bottom, the elbow).")
    L.append(f"constexpr int HIP_Y = {hip}, LEG_SPLIT = {split}, EAR_Y = {ear}, EAR_SPLIT = {ear_split};")
    L.append(f"constexpr int ARM_X0 = {ax0}, ARM_Y0 = {ay0}, ARM_X1 = {ax1}, ARM_Y1 = {ay1};")
    L.append(f"constexpr int NPAL = {len(palette) + 1};")
    L.append("enum : uint8_t { TAG_NONE = 0, TAG_VISOR = 1, TAG_LED = 2, TAG_RIM = 3, TAG_MOUTH = 4 };")
    L.append("static const uint32_t PAL888[NPAL] = {0x000000,")
    for i in range(0, len(palette), 8):
        L.append("  " + ", ".join(f"0x{r:02x}{g:02x}{b:02x}" for r, g, b in palette[i:i + 8]) + ",")
    L.append("};")
    L.append("static const uint16_t PAL565[NPAL] = {0x0000,")
    for i in range(0, len(palette), 10):
        L.append("  " + ", ".join(f"0x{c565(c):04x}" for c in palette[i:i + 10]) + ",")
    L.append("};")
    L.append("static const uint8_t TAG[NPAL] = {0,")
    for i in range(0, len(tags), 32):
        L.append("  " + ", ".join(str(t) for t in tags[i:i + 32]) + ",")
    L.append("};")
    L.append("// The pixels are in rabbit_sprite.cpp, so there's one copy in flash however many files draw him.")
    L.append("extern const uint8_t PIX[NFRAMES][W * H];")
    L.append("}  // namespace rabbitsprite")
    with open(a.out, "w") as f:
        f.write("\n".join(L) + "\n")
    C = ["// Generated by tools/make_sprite.py - edit the art, not this.",
         '#include "rabbit_sprite.h"', "", "namespace rabbitsprite {",
         "const uint8_t PIX[NFRAMES][W * H] = {"]
    for k, d in enumerate(datas):
        C.append(f"  {{  // frame {k}")
        for y in range(H):
            C.append("    " + ",".join(str(v) for v in d[y * W:(y + 1) * W]) + ",")
        C.append("  },")
    C.append("};")
    C.append("}  // namespace rabbitsprite")
    cpp = os.path.splitext(a.out)[0] + ".cpp"
    with open(cpp, "w") as f:
        f.write("\n".join(C) + "\n")
    if walk:
        print("planted foot (x0, x1, dx) per walk drawing:", stance, " stride", stride_px, "px, knee", knee)
        print("trailing boot (x0, x1) per walk drawing:", trail)
    print(f"rig: hip {hip}, split {split}, ears {ear} (split at {ear_split}), arm {ax0},{ay0}-{ax1},{ay1}")
    print(f"{a.out} + {os.path.basename(cpp)}: {n} frame{'s' if n > 1 else ''} of {W}x{H}, {len(palette)} colours, visor at {vx},{vy}, "
          f"led at {lx},{ly}, {n * W * H} bytes of pixels")


if __name__ == "__main__":
    main()
