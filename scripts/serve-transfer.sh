#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRANSFER_DIR="${1:-$ROOT/transfer}"
PORT="${TRANSFER_PORT:-8000}"

mkdir -p "$TRANSFER_DIR"
cd "$TRANSFER_DIR"

echo "Serving $TRANSFER_DIR"
echo "From XP, open: http://10.0.2.2:$PORT/"
exec python3 -m http.server "$PORT" --bind 127.0.0.1
