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

open -a "$VECX_APP" "$ROOT/$ROM"
echo "Launched $ROM in Vecx"
