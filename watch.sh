#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "Watching src/main.c and Makefile — Ctrl+C to stop"
echo "Each change will rebuild and launch Vecx."

last=""
while true; do
  current=$(stat -f "%m %N" src/main.c Makefile config.env 2>/dev/null | tr '\n' '|')
  if [[ -n "$current" && "$current" != "$last" ]]; then
    if [[ -n "$last" ]]; then
      echo ""
      echo "── change detected, rebuilding ──"
      "$ROOT/run.sh" || echo "build/run failed (fix errors and save again)"
    else
      "$ROOT/run.sh"
    fi
    last="$current"
  fi
  sleep 1
done
