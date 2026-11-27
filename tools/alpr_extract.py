#!/usr/bin/env python3
"""Build RabbitMesh's Flock-hunt camera map on a computer, for a whole state, region or country.

The T-Deck can download its own map (Deck > FLOCK > update camera map), but only keeps the
cameras within a few hundred km of where it is. This script makes the same file, cameras.bin,
for any area you choose; copy it to the SD card as /rabbit/alpr/cameras.bin.

Where the data comes from: ALPR cameras mapped on OpenStreetMap (the DeFlock community's
tagging), published once a day as one static file by the deflock-data project
(github.com/flockhopper3/deflock-data). This script downloads that one file, gzip-compressed
(~4 MB for the US), and keeps a copy: it won't fetch it again within 20 hours unless you ask.
It never queries the Overpass API. Map data (c) OpenStreetMap contributors, ODbL.

Examples:
    python3 alpr_extract.py --near 36.85,-76.29 --radius-km 300         # round Norfolk, VA
    python3 alpr_extract.py --bbox 36.5,-83.7,39.5,-75.2                  # Virginia's box
    python3 alpr_extract.py --all                                         # the whole US (~4.7 MB)
    python3 alpr_extract.py --country ca --all
    python3 alpr_extract.py --from cameras.geojson.gz --all               # a file you already have

Only Python 3's standard library is needed.
"""
import argparse
import gzip
import io
import json
import math
import os
import struct
import sys
import time
import urllib.request

VERSION = "0.1.0"
URLS = {
    "us": "https://data.dontgetflocked.com/cameras.geojson.gz",
    "ca": "https://data.dontgetflocked.com/cameras-ca.geojson.gz",
}
USER_AGENT = f"RabbitMesh-alpr-extract/{VERSION}"
REFETCH_S = 20 * 3600
MAGIC = b"RMALPR1\0"

# The same as src/alpr_core.h (brandCode, Record, cellKey, metres): the T-Deck reads what this writes.
BRANDS = [("flock", 1), ("motorola", 2), ("genetec", 3), ("axis", 4), ("leonardo", 5), ("rekor", 6), ("axon", 7)]


def brand_code(s):
    if not s:
        return 0
    raw = s.encode("utf-8")[:23]
    low = bytes(b + 32 if 65 <= b <= 90 else b for b in raw)
    for key, code in BRANDS:
        if key.encode() in low:
            return code
    return 8


def clean(s, cap):
    """A string value as the T-Deck's scanner keeps it: control characters as spaces, in cap-1 bytes."""
    out = "".join(" " if ord(c) < 32 else c for c in s)
    return out.encode("utf-8")[:cap - 1]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def lround(v):
    return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))


def cell_key(lat, lon):
    return (math.floor(lat) + 90) * 360 + (math.floor(lon) + 180)


def metres(la1, lo1, la2, lo2):
    r, k = 6371000.0, 3.14159265358979 / 180.0
    dla, dlo = (la2 - la1) * k, (lo2 - lo1) * k
    a = math.sin(dla / 2) ** 2 + math.cos(la1 * k) * math.cos(la2 * k) * math.sin(dlo / 2) ** 2
    return 2 * r * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def record(feature):
    """(lat, lon, dir, brand, operator bytes), or None for a feature the T-Deck would skip."""
    try:
        lon, lat = feature["geometry"]["coordinates"][:2]
        lon, lat = float(lon), float(lat)
    except (KeyError, TypeError, ValueError, IndexError):
        return None
    if lat < -90 or lat > 90 or lon < -180 or lon > 180 or (lat == 0 and lon == 0):
        return None
    p = feature.get("properties") or {}
    d = p.get("direction")
    direction = (lround(float(d)) % 360 + 360) % 360 if isinstance(d, (int, float)) else -1
    brand = brand_code(clean(p["brand"], 64).decode("utf-8", "ignore")) if isinstance(p.get("brand"), str) else 0
    op = clean(p["operator"], 64)[:19] if isinstance(p.get("operator"), str) else b""
    return (f32(lat), f32(lon), direction, brand, op)


def fetch(country, cache, force):
    if os.path.exists(cache) and not force and time.time() - os.path.getmtime(cache) < REFETCH_S:
        print(f"using {cache} (fetched {int((time.time() - os.path.getmtime(cache)) / 3600)} h ago; --refresh to fetch again)")
        return
    print(f"downloading {URLS[country]} ...")
    req = urllib.request.Request(URLS[country], headers={"User-Agent": USER_AGENT, "Accept-Encoding": "gzip"})
    with urllib.request.urlopen(req, timeout=120) as r:
        body = r.read()
        if r.headers.get("Content-Encoding") != "gzip" and body[:2] != b"\x1f\x8b":
            body = gzip.compress(body)          # keep the cache compressed either way
    tmp = cache + ".part"
    with open(tmp, "wb") as f:
        f.write(body)
    os.replace(tmp, cache)
    print(f"  {len(body) / 1e6:.1f} MB saved to {cache}")


def load(path):
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    return json.loads(raw)


def write(records, path, built):
    # the same full order as src/alpr_core.h (cameras sharing a pole too), so both write the same bytes
    records.sort(key=lambda r: (cell_key(r[0], r[1]), r[0], r[1], r[2], r[3], r[4].ljust(20, b"\0")))
    cells = []
    for i, r in enumerate(records):
        k = cell_key(r[0], r[1])
        if not cells or cells[-1][0] != k:
            cells.append([k, i, 0])
        cells[-1][2] += 1
    out = io.BytesIO()
    out.write(MAGIC + struct.pack("<IIII", len(records), len(cells), built, 0))
    for k, first, n in cells:
        out.write(struct.pack("<iII", k, first, n))
    for lat, lon, d, b, op in records:
        out.write(struct.pack("<ffhBB20s", lat, lon, d, b, 0, op))
    with open(path, "wb") as f:
        f.write(out.getvalue())
    return len(cells), out.tell()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    where = ap.add_mutually_exclusive_group(required=True)
    where.add_argument("--near", metavar="LAT,LON", help="keep cameras round this point (with --radius-km)")
    where.add_argument("--bbox", metavar="S,W,N,E", help="keep cameras in this box (degrees)")
    where.add_argument("--all", action="store_true", help="keep every camera in the country's file")
    ap.add_argument("--radius-km", type=float, default=250, help="with --near (default 250)")
    ap.add_argument("--country", choices=sorted(URLS), default="us")
    ap.add_argument("--flock-only", action="store_true", help="only Flock Safety cameras")
    ap.add_argument("--from", dest="src", metavar="FILE", help="a cameras GeoJSON (.gz or not) you already have")
    ap.add_argument("--cache", help="where to keep the download (default: next to this script)")
    ap.add_argument("--refresh", action="store_true", help="fetch again even if the copy is under 20 hours old")
    ap.add_argument("--built", type=int, help=argparse.SUPPRESS)       # for byte-for-byte tests
    ap.add_argument("-o", "--out", default="cameras.bin", help="output (default cameras.bin)")
    a = ap.parse_args()

    src = a.src
    if not src:
        src = a.cache or os.path.join(os.path.dirname(os.path.abspath(__file__)), f"cameras-{a.country}.geojson.gz")
        try:
            fetch(a.country, src, a.refresh)
        except Exception as e:  # noqa: BLE001 - say what went wrong, plainly
            sys.exit(f"download failed: {e}")
    data = load(src)
    feats = data.get("features") or []

    if a.near:
        lat0, lon0 = (float(v) for v in a.near.split(","))
        keep = lambda r: abs(r[0] - lat0) * 111.0 <= a.radius_km and metres(lat0, lon0, r[0], r[1]) <= a.radius_km * 1000
    elif a.bbox:
        s, w, n, e = (float(v) for v in a.bbox.split(","))
        keep = lambda r: s <= r[0] <= n and w <= r[1] <= e
    else:
        keep = lambda r: True

    records = []
    skipped = 0
    for f in feats:
        r = record(f)
        if r is None:
            skipped += 1
            continue
        if a.flock_only and r[3] != 1:
            continue
        if keep(r):
            records.append(r)
    if not records:
        sys.exit("no cameras in that area: nothing written")
    cells, size = write(records, a.out, a.built if a.built is not None else int(time.time()))
    print(f"{len(records)} cameras ({sum(1 for r in records if r[3] == 1)} Flock) of {len(feats)} in the file, "
          f"{cells} one-degree cells, {size / 1e6:.2f} MB -> {a.out}")
    if skipped:
        print(f"  ({skipped} features without a usable position were skipped)")
    print("copy it to the T-Deck's SD card as /rabbit/alpr/cameras.bin")
    print("map data (c) OpenStreetMap contributors, ODbL")


if __name__ == "__main__":
    main()
