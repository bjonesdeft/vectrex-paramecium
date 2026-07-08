#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# shellcheck disable=SC1091
source "$ROOT/config.env"

export VECTREC
export PATH="$VECTREC:$PATH"

if [[ ! -x "$VECTREC/cmoc" ]]; then
  echo "error: VectreC not found at $VECTREC" >&2
  echo "Set VECTREC in config.env or install from https://github.com/rogerboesch/vectreC" >&2
  exit 1
fi

make

echo "Built $ROM"
