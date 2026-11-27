# Draws the recorded text over each rendered screen and scales it up (preview only).
import sys, glob
from PIL import Image, ImageDraw, ImageFont
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
SIZES = {0: (MONO, 8, 0), 2: (MONO, 12, 2), 4: (BOLD, 21, 2), 6: (BOLD, 40, 2)}
S = 3
for ppm in sorted(glob.glob(sys.argv[1] + "/*.ppm")):
    im = Image.open(ppm).convert("RGB").resize((320 * S, 240 * S), Image.NEAREST)
    dr = ImageDraw.Draw(im)
    try: ops = open(ppm[:-4] + ".txt").read().splitlines()
    except FileNotFoundError: ops = []
    for line in ops:
        x, y, f, c, text = line.split("\t", 4)
        c = int(c); rgb = ((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)
        path, size, dy = SIZES.get(int(f), SIZES[2])
        dr.text((int(x) * S, (int(y) + dy) * S), text, fill=rgb, font=ImageFont.truetype(path, size * S))
    im.save(ppm[:-4] + ".png")
