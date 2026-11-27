#!/usr/bin/env python3
"""The holiday costume frames (art/holiday/<name>/, from tools/make_holiday_frames.py) ->
src/holiday_sprite.h + .cpp: one complete sprite set per holiday, drawn by rabbit.h in place of
the base frames while that holiday's theme is up.

    python3 tools/make_holiday_sprites.py

Every set is cut with the base sprite's own crop box and scale (rabbit_sprite.h: W x H), so its
pixels sit exactly where the base frames' do and the base rig, planted-foot and ground data hold
for it. Each set has its own palette (at most 59 colours, plus the visor, the radio's screen, the
mouth and the rim: 64 entries), and its frames are stored as one-byte runs (a 2-bit length and a
6-bit colour; a second byte for long runs), about 3.5 KB a frame, unpacked into one frame buffer
as they're drawn. Needs Pillow + numpy.
"""
import glob
import os
import re
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_sprite as ms  # noqa: E402

ROOT = os.path.join(HERE, "..")
NAMES = ["halloween", "thanksgiving", "christmas", "newyear", "july4"]
FRAMES = [f"walk_{i}" for i in range(8)] + [f"talk_{i}" for i in range(4)]
H0 = 122
COLOURS = 59                     # + visor, screen, mouth, rim and transparent: 64 entries, a 6-bit index


def base_box():
    """make_sprite.py's crop box: round the standing art and every base frame."""
    ims = [Image.open(os.path.join(ROOT, "art", "rabbit.png")).convert("RGBA")]
    for d, pat in (("walk", "walk_*.png"), ("talk", "talk_*.png")):
        ims += [Image.open(p).convert("RGBA") for p in glob.glob(os.path.join(ROOT, "art", d, pat))]
    bb = [im.getchannel("A").point(lambda v: 255 if v > 32 else 0).getbbox() for im in ims]
    return (min(b[0] for b in bb), min(b[1] for b in bb), max(b[2] for b in bb), max(b[3] for b in bb))


def c565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def build(name, box, W0):
    d = os.path.join(ROOT, "art", "holiday", name)
    W, H = W0 + 2, H0 + 2
    smalls, covers = [], []
    for f in FRAMES:
        full = Image.open(os.path.join(d, f + ".png")).convert("RGBA").crop(box)
        if full.size[0] > 0 and Image.open(os.path.join(d, f + ".png")).getchannel("A").crop(box).getbbox() is None:
            raise SystemExit(f"{name}/{f}: empty")
        small = full.convert("RGBa").resize((W0, H0), Image.BOX).convert("RGBA")
        tg = np.array(Image.open(os.path.join(d, "tags_" + f + ".png")).crop(box))
        cov = {t: np.array(Image.fromarray(((tg == t) * 255).astype(np.uint8)).resize((W0, H0), Image.BOX))
               for t in (ms.TAG_VISOR, ms.TAG_LED, ms.TAG_MOUTH)}
        smalls.append(np.array(small))
        covers.append(cov)
    # one palette for the set
    sheet = np.zeros((H0, W0 * len(FRAMES), 3), np.uint8)
    for i, s in enumerate(smalls):
        op = s[:, :, 3] >= 128
        sheet[:, i * W0:(i + 1) * W0][op] = s[:, :, :3][op]
    q = Image.fromarray(sheet).quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    qa = np.array(q)
    pal = q.getpalette()[: COLOURS * 3]
    used = sorted({int(qa[y, i * W0 + x]) for i, s in enumerate(smalls) for y, x in zip(*np.where(s[:, :, 3] >= 128))})
    remap = {o: n + 1 for n, o in enumerate(used)}
    palette = [tuple(pal[o * 3:o * 3 + 3]) for o in used]
    tags = [ms.TAG_NONE] * len(palette)
    glow = {}
    for t, thr in ((ms.TAG_VISOR, 85), (ms.TAG_LED, 85), (ms.TAG_MOUTH, 30)):
        # the brightest full-size colour under the tag, over all the frames
        best, bv = None, -1
        for f in FRAMES:
            full = np.array(Image.open(os.path.join(d, f + ".png")).convert("RGBA").crop(box)).astype(int)
            tg = np.array(Image.open(os.path.join(d, "tags_" + f + ".png")).crop(box))
            m = (tg == t) & (full[:, :, 3] >= 128)
            if m.any():
                px = full[m][:, :3]
                k = px.sum(1).argmax() if t != ms.TAG_MOUTH else np.abs(px[:, 0] - px[:, 1]).argmax()
                if px[k].sum() > bv:
                    bv, best = px[k].sum(), tuple(int(v) for v in px[k])
        if best:
            palette.append(best)
            tags.append(t)
            glow[t] = (len(palette), thr)
    fur = 1 + max(range(len(used)), key=lambda n: sum(palette[n]))
    palette.append((0, 0, 0))
    tags.append(ms.TAG_RIM)
    rim = len(palette)
    if len(palette) + 1 > 64:
        raise SystemExit(f"{name}: {len(palette) + 1} palette entries, a run holds a 6-bit colour")
    datas = []
    vis_pts = []
    for i, s in enumerate(smalls):
        dd = [0] * (W * H)
        for y in range(H0):
            for x in range(W0):
                if s[y, x, 3] >= 128:
                    v = remap[int(qa[y, i * W0 + x])]
                    for t, (idx, thr) in glow.items():
                        if covers[i][t][y, x] >= thr:
                            v = idx
                    dd[(y + 1) * W + x + 1] = v
                    if i == 0 and covers[i][ms.TAG_VISOR][y, x] >= 85:
                        vis_pts.append((x + 1, y + 1))
        ms.seal(dd, W, H, max_w=3)
        low = max((y for y in range(H) if any(dd[y * W:(y + 1) * W])), default=H - 2)
        if 0 < H - 2 - low:
            k = H - 2 - low
            dd = [0] * (W * k) + dd[: W * (H - k)]
        out = list(dd)
        for y in range(H):
            for x in range(W):
                if dd[y * W + x]:
                    continue
                for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + ddx, y + ddy
                    if 0 <= xx < W and 0 <= yy < H and dd[yy * W + xx]:
                        out[y * W + x] = rim
                        break
        ms.seal(out, W, H, max_w=1)
        datas.append(out)
    vx = sum(p[0] for p in vis_pts) // len(vis_pts)
    vy = sum(p[1] for p in vis_pts) // len(vis_pts)
    return W, H, palette, tags, fur, datas, (vx, vy)


def rle(d):
    """Runs as one byte each, (length code << 6) | colour: codes 0..2 are 1..3 pixels, code 3
    takes the next byte too, 4 + that pixels (up to 259)."""
    out, i = [], 0
    while i < len(d):
        v, n = d[i], 1
        while i + n < len(d) and d[i + n] == v and n < 259:
            n += 1
        if n <= 3:
            out.append(((n - 1) << 6) | v)
        else:
            out += [(3 << 6) | v, n - 4]
        i += n
    return out


def unrle(r, n):
    out, i = [], 0
    while len(out) < n:
        c, v = r[i] >> 6, r[i] & 63
        i += 1
        if c < 3:
            out += [v] * (c + 1)
        else:
            out += [v] * (4 + r[i])
            i += 1
    return out


def main():
    box = base_box()
    W0 = max(1, round((box[2] - box[0]) * H0 / (box[3] - box[1])))
    hdr = open(os.path.join(ROOT, "src", "rabbit_sprite.h")).read()
    bw, bh = (int(v) for v in re.search(r"constexpr int W = (\d+), H = (\d+);", hdr).groups())
    if (bw, bh) != (W0 + 2, H0 + 2):
        raise SystemExit(f"the base sprite is {bw}x{bh}, this box gives {W0 + 2}x{H0 + 2}: run make_sprite.py first")
    Hh = ["// Generated by tools/make_holiday_sprites.py from art/holiday - edit the art, not this.",
          "// The rabbit in each holiday's costume: complete sprite sets on the base sprite's canvas",
          "// (rabbit_sprite.h W x H, frames in the same order, the same rig), each with its own palette.",
          "#pragma once", "#include <stdint.h>", "", "namespace holidaysprite {", "",
          "struct Set {",
          "  const char* name;",
          "  uint8_t npal, furIdx;            // palette entries (0 is transparent), the colour his mouth closes to",
          "  int8_t visorX, visorY;           // the visor's centre, in sprite pixels",
          "  const uint16_t* pal565;",
          "  const uint8_t* tag;              // rabbitsprite::TAG_* per entry",
          "  const uint32_t* frameAt;         // where each frame's runs start in `runs` (NFRAMES + 1)",
          "  const uint8_t* runs;             // row by row: (length code << 6) | colour; code 3: 4 + the next byte",
          "};", ""]
    C = ["// Generated by tools/make_holiday_sprites.py - edit the art, not this.",
         '#include "holiday_sprite.h"', "", "namespace holidaysprite {", ""]
    total = 0
    for n in NAMES:
        W, H, palette, tags, fur, datas, (vx, vy) = build(n, box, W0)
        N = n.upper()
        runs, at = [], [0]
        for dd in datas:
            r = rle(dd)
            assert unrle(r, len(dd)) == dd
            runs += r
            at.append(len(runs))
        total += len(runs) + len(palette) * 3
        C.append(f"static const uint16_t {N}_PAL[] = {{0x0000, " + ", ".join(f"0x{c565(c):04x}" for c in palette) + "};")
        C.append(f"static const uint8_t {N}_TAG[] = {{0, " + ", ".join(str(t) for t in tags) + "};")
        C.append(f"static const uint32_t {N}_AT[] = {{" + ", ".join(str(v) for v in at) + "};")
        C.append(f"static const uint8_t {N}_RUNS[] = {{")
        for i in range(0, len(runs), 40):
            C.append("  " + ",".join(str(v) for v in runs[i:i + 40]) + ",")
        C.append("};")
        C.append(f'const Set {N} = {{"{n}", {len(palette) + 1}, {fur}, {vx}, {vy}, {N}_PAL, {N}_TAG, {N}_AT, {N}_RUNS}};')
        C.append("")
        Hh.append(f"extern const Set {N};")
        print(f"{n}: {len(palette)} colours, visor at {vx},{vy}, {len(runs)} bytes of runs")
    Hh += ["", "}  // namespace holidaysprite"]
    C.append("}  // namespace holidaysprite")
    with open(os.path.join(ROOT, "src", "holiday_sprite.h"), "w") as f:
        f.write("\n".join(Hh) + "\n")
    with open(os.path.join(ROOT, "src", "holiday_sprite.cpp"), "w") as f:
        f.write("\n".join(C) + "\n")
    print(f"src/holiday_sprite.h + .cpp: {total} bytes")


if __name__ == "__main__":
    main()
