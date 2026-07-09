#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# shellcheck disable=SC1091
source "$ROOT/config.env"

"$ROOT/compile.sh"

if [[ ! -d "$VECX_APP" ]]; then
  echo "error: Vecx not found at $VECX_APP" >&2
  echo "Set VECX_APP in config.env to your Vecx.app path" >&2
  exit 1
fi

if [[ ! -f "$ROM" ]]; then
  echo "error: ROM missing at $ROM" >&2
  exit 1
fi

# Clear quarantine on unsigned Vecx builds if needed
xattr -cr "$VECX_APP" 2>/dev/null || true

# Force-quit any running Vecx so it can't keep a stale ROM mapped, then
# open a uniquely-named copy so macOS/Vecx can't reuse a cached binary.
osascript -e 'quit app "Vecx"' 2>/dev/null || true
pkill -f "Vecx.app/Contents/MacOS/Vecx" 2>/dev/null || true
sleep 0.4

STAMP="$(date +%H%M%S)"
LAUNCH_ROM="build/sphere-link-${STAMP}.bin"
cp "$ROM" "$LAUNCH_ROM"
# Keep only the newest few stamped copies so build/ doesn't grow forever.
ls -1t build/sphere-link-*.bin 2>/dev/null | tail -n +4 | xargs rm -f 2>/dev/null || true

open -a "$VECX_APP" "$ROOT/$LAUNCH_ROM"
echo "Launched $LAUNCH_ROM in Vecx"
