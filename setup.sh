#!/usr/bin/env bash
# Builds the RabbitMesh source tree: Squatch Mesh's T-Deck firmware at a pinned commit,
# with this folder's patch applied and its new files copied in. Upstream code is fetched,
# not kept in this repo.
#
#   ./setup.sh            -> ./build
#   ./setup.sh /some/dir  -> /some/dir
#   then: cd build && pio run -e t-deck-rabbit
set -euo pipefail

UPSTREAM=https://github.com/BamBam1121/inw-mesh.git
TAG=tdeck-v1.2.7
COMMIT=cf32120ab35a8c231a5a51d61201af6a65b5288a

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/build}

if [ -e "$OUT" ]; then
  echo "$OUT already exists: remove it, or pass another directory" >&2
  exit 1
fi

git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$TAG" "$UPSTREAM" "$OUT"
got=$(git -C "$OUT" rev-parse HEAD)
if [ "$got" != "$COMMIT" ]; then
  echo "tag $TAG is now $got, not the $COMMIT this patch was made against - check before building" >&2
  exit 1
fi

git -C "$OUT" apply --whitespace=nowarn "$HERE/patches/rabbitmesh-tdeck.patch"
cp "$HERE"/src/* "$OUT/src/"

echo "RabbitMesh source ready in $OUT"
echo "  build:  cd $OUT && pio run -e t-deck-rabbit"
echo "  flash:  cd $OUT && pio run -e t-deck-rabbit -t upload   (hold the trackball while powering on for download mode)"
