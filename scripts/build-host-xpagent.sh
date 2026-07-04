#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/host/xpagent"

mkdir -p "$(dirname "$OUT")"
cc -std=c89 -Wall -Wextra -pedantic \
  "$ROOT/portable/agent_core.c" \
  "$ROOT/host/xpagent_posix.c" \
  -o "$OUT"

echo "Wrote $OUT"
