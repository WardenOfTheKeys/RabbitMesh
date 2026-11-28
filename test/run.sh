#!/usr/bin/env bash
# Host tests for the wardrive, deck, spectrum, MECP, fox hunt, WiGLE, Flock signature, holiday, time zone, mesh clock, game and camera-map logic (no hardware, no PlatformIO).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
for t in wardrive_core_test deck_core_test spectrum_core_test mecp_core_test fox_core_test wigle_core_test flock_core_test holiday_core_test tz_core_test meshclock_core_test bricks_core_test updcore_test; do
  g++ -std=c++17 -Wall -Wextra -Wno-unused-function -I"$HERE/../src" "$HERE/$t.cpp" -o "$OUT/$t"
  "$OUT/$t"
done
# The update signature, with the T-Deck's own Ed25519 code (MeshCore's), once a build has fetched it.
ED="$HERE/../.pio/libdeps/t-deck-rabbit/MeshCore/lib/ed25519"
if [ -d "$ED" ]; then
  (cd "$OUT" && gcc -O2 -w -c -I"$ED" "$ED"/{add_scalar,fe,ge,keypair,sc,seed,sha512,sign,verify,key_exchange}.c)
  g++ -std=c++17 -Wall -Wextra -DWITH_ED25519 -I"$HERE/../src" -I"$ED" "$HERE/updcore_test.cpp" "$OUT"/*.o -o "$OUT/updcore_sig_test"
  "$OUT/updcore_sig_test"
else
  echo "(the signature check runs once a build has fetched MeshCore: pio run -e t-deck-rabbit)"
fi
# The camera map: with miniz's inflate (the T-Deck uses the ROM's copy). Set ALPR_GZ to a copy
# of https://data.dontgetflocked.com/cameras.geojson.gz to also run the real file through it.
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-function -I"$HERE/../src" -I"$HERE/third_party/miniz" \
  "$HERE/alpr_core_test.cpp" "$HERE/third_party/miniz/miniz_tinfl.c" -o "$OUT/alpr_core_test"
"$OUT/alpr_core_test"
