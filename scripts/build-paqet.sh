#!/usr/bin/env bash
# Build paqet binary for desktop (Linux/macOS).
# Usage: ./build-paqet.sh [path/to/paqet]
# Requires: Go. Paqet source at argument or $PAQET_SRC or ../paqet.
# Output: paqet in build/paqet/; copy next to the app executable.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
PAQET_SRC="${1:-${PAQET_SRC:-$REPO_ROOT/paqet}}"
if [[ ! -f "$PAQET_SRC/go.mod" ]]; then
  echo "paqet source not found at $PAQET_SRC" >&2
  exit 1
fi
BUILD_DIR="$REPO_ROOT/build/paqet"
mkdir -p "$BUILD_DIR"
case "$(uname -s)" in
  Linux)   GOOS=linux ;;
  Darwin)  GOOS=darwin ;;
  *)       echo "Unsupported OS" >&2; exit 1 ;;
esac
GOARCH=amd64
OUTPUT="$BUILD_DIR/paqet"
echo "Building paqet for $GOOS/$GOARCH into $OUTPUT"
(cd "$PAQET_SRC" && GOOS=$GOOS GOARCH=$GOARCH go build -o "$OUTPUT" ./cmd/...)
echo "Done. Copy $OUTPUT next to your app executable."
