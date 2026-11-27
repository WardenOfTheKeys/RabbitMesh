#!/usr/bin/env python3
"""Builds walk frames from the standing art: art/rabbit.png -> art/walk/walk_0..7.png.

    python3 tools/make_walk.py [art/rabbit.png] [art/walk] [--frames 8]

Not hand-drawn animation: it cuts the legs out of the full-size painting and turns them
at real joints, the thigh about the hip and the shin about the knee, with smooth
resampling, before the frames are shrunk to sprite size. At full size a rotation keeps
the boots and folds of the trousers looking like themselves, where bending the small
sprite only shears them. Each frame is then dropped so the lowest boot stands on the
ground - the planted boot, not the passing one, whose toe tips down as its knee bends -
which gives the body its bob (lowest with the legs apart, highest as they pass).

The walk: contact - passing - contact - passing. At each contact the legs open, his left
(our left) thigh back and his right forward, SWING degrees each about the hip; in a 3/4
view that reads on both halves of the stride, where swinging them the other way crosses
the legs and hides one behind the other. At each passing they come together and the leg
coming through bends its knee up to KNEE degrees, kicking the shin back - his left leg on
one passing, his right on the next. The passing leg is drawn in front.

Hand-drawn frames can replace these: put walk_0.png ... walk_N.png in art/walk (same
canvas size as the standing art, feet on the same ground line) and skip this script.
tools/make_sprite.py uses whatever is in art/walk.

The joints are for art/rabbit.png; for other art, change JOINTS below. Needs Pillow + numpy.
"""
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else os.path.join(HERE, "..", "art", "rabbit.png")
OUT = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else os.path.join(HERE, "..", "art", "walk")
N = int(sys.argv[sys.argv.index("--frames") + 1]) if "--frames" in sys.argv else 8

# Full-size pixel positions in art/rabbit.png (941 x 1672).
JOINTS = {
    "hem_y": 1050,          # the jacket's hem: above it is the body, which doesn't move
    "split_x": 425,         # between the legs, below the hem
    "hip_l": (300, 1072),   # where each thigh turns (his left leg is on our left)
    "hip_r": (528, 1072),
    "knee_y": 1378,         # where each shin turns
}
SWING = 16.0                # degrees, thigh forward/back
KNEE = 30.0                 # degrees, the passing leg's knee
LIFT = 8.0                  # degrees more forward for the passing thigh, so its foot clears the ground


def rot(px, py, cx, cy, deg):
    """A point turned about (cx, cy) the way PIL's rotate(deg) turns the image (anticlockwise
    on screen, y down): a point below the centre moves right for deg > 0."""
    a = math.radians(deg)
    dx, dy = px - cx, py - cy
    return cx + dx * math.cos(a) + dy * math.sin(a), cy - dx * math.sin(a) + dy * math.cos(a)


def layer(base, mask):
    a = np.array(base)
    a[..., 3] = np.where(mask, a[..., 3], 0)
    return Image.fromarray(a, "RGBA")


def main():
    base = Image.open(SRC).convert("RGBA")
    W, H = base.size
    ys, xs = np.mgrid[0:H, 0:W]
    j = JOINTS
    legs = ys >= j["hem_y"]
    # The legs start a little under the hem: the body is drawn over them, so when a leg
    # turns no seam opens at the hip for the rim light to show in.
    under = ys >= j["hem_y"] - 30
    leg_l, leg_r = under & (xs < j["split_x"]), under & (xs >= j["split_x"])
    body = layer(base, ~legs)
    ground = int(np.nonzero(np.array(base)[..., 3] > 128)[0].max())
    os.makedirs(OUT, exist_ok=True)

    def turned_leg(mask, hip, thigh_deg, knee_deg):
        hx, hy = hip
        ky = j["knee_y"]
        kx = float(np.median(xs[mask & (ys == ky)])) if (mask & (ys == ky)).any() else hx
        thigh = layer(base, mask & (ys < ky + 24))            # overlaps the shin at the knee
        shin = layer(base, mask & (ys >= ky - 12))
        t = thigh.rotate(thigh_deg, resample=Image.BICUBIC, center=(hx, hy))
        nkx, nky = rot(kx, ky, hx, hy, thigh_deg)              # where the knee went
        s = shin.rotate(thigh_deg - knee_deg, resample=Image.BICUBIC, center=(kx, ky),
                        translate=(nkx - kx, nky - ky))
        out = Image.new("RGBA", base.size)
        out.alpha_composite(t)
        out.alpha_composite(s)
        return out

    for k in range(N):
        ph = 2 * math.pi * k / N
        sn, cs = math.sin(ph), math.cos(ph)
        # his left leg leads with sin, the right is half a stride behind; the leg coming
        # through (moving forward) bends its knee.
        pl, pr = max(0.0, cs), max(0.0, -cs)                   # how much each leg is passing
        spread = SWING * abs(sn)
        lft = turned_leg(leg_l, j["hip_l"], -spread + LIFT * pl, KNEE * pl)
        rgt = turned_leg(leg_r, j["hip_r"], spread + LIFT * pr, KNEE * pr)
        frame = Image.new("RGBA", base.size)
        back, front = (rgt, lft) if cs > 0 else (lft, rgt)    # the passing leg in front
        frame.alpha_composite(back)
        frame.alpha_composite(front)
        frame.alpha_composite(body)
        # Feet on the ground: the planted leg's lowest point goes to the ground line.
        planted = back
        low = int(np.nonzero(np.array(planted)[..., 3] > 128)[0].max())
        if low != ground:
            shifted = Image.new("RGBA", base.size)
            shifted.alpha_composite(frame, (0, max(0, ground - low)) if low < ground else (0, 0))
            if low > ground:                                  # a planted toe below the line: lift it all
                shifted = Image.new("RGBA", base.size)
                shifted.paste(frame.crop((0, low - ground, base.size[0], base.size[1])), (0, 0))
            frame = shifted
        path = os.path.join(OUT, f"walk_{k}.png")
        frame.save(path)
        print(f"{path}: phase {math.degrees(ph):5.1f} deg, dropped {ground - low} px")


if __name__ == "__main__":
    main()
