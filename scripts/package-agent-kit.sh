#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/agent-kit"
TRANSFER_DIR="$ROOT/transfer"
TCC_ZIP="$TRANSFER_DIR/tools/tcc-0.9.27-win32-bin.zip"
WINAPI_ZIP="$TRANSFER_DIR/tools/winapi-full-for-0.9.27.zip"
OUT="$TRANSFER_DIR/agent-kit.zip"

if [[ ! -f "$TCC_ZIP" || ! -f "$WINAPI_ZIP" ]]; then
  "$ROOT/scripts/fetch-tools.sh"
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
trap 'rm -rf "$BUILD_DIR"' EXIT

unzip -q "$TCC_ZIP" -d "$BUILD_DIR"
unzip -q "$WINAPI_ZIP" -d "$BUILD_DIR/_winapi"

mkdir -p "$BUILD_DIR/tcc/include"
cp -R "$BUILD_DIR/_winapi/winapi-full-for-0.9.27/include/"* "$BUILD_DIR/tcc/include/"
rm -rf "$BUILD_DIR/_winapi"

mkdir -p "$BUILD_DIR/agent"
cp "$ROOT"/guest/* "$BUILD_DIR/agent/"
perl -0pi -e 's/\r?\n/\r\n/g' "$BUILD_DIR"/agent/*.bat "$BUILD_DIR"/agent/README-XP.txt

rm -f "$OUT"
(cd "$BUILD_DIR" && zip -qr "$OUT" tcc agent)

echo "Wrote $OUT"
