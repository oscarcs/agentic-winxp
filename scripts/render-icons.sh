#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG_DIR="$ROOT/guest/assets/icons/svg"
BMP_DIR="$ROOT/guest/assets/icons/bmp"
ICO_DIR="$ROOT/guest/assets/icons/ico"
TMP_DIR="$(mktemp -d)"

trap 'rm -rf "$TMP_DIR"' EXIT

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing $1. Install librsvg and imagemagick." >&2
    exit 1
  fi
}

render_bmp() {
  local name="$1"
  local size="$2"
  local png="$TMP_DIR/$name-$size.png"

  rsvg-convert -w "$size" -h "$size" "$SVG_DIR/$name.svg" -o "$png"
  magick "$png" -alpha on "$BMP_DIR/$name.bmp"
}

render_ico() {
  local name="$1"
  local size="$2"
  local png="$TMP_DIR/$name-$size-icon.png"

  rsvg-convert -w "$size" -h "$size" "$SVG_DIR/$name.svg" -o "$png"
  magick "$png" "$ICO_DIR/$name.ico"
}

need rsvg-convert
need magick

mkdir -p "$BMP_DIR" "$ICO_DIR"

for name in new-chat search scheduled plugins add mic send file project-open project-closed; do
  render_bmp "$name" 16
  render_ico "$name" 16
done

render_bmp xpagent-badge 32
render_ico xpagent-badge 32

rsvg-convert -w 16 -h 16 "$SVG_DIR/xpagent-badge.svg" -o "$TMP_DIR/xpagent-16.png"
rsvg-convert -w 32 -h 32 "$SVG_DIR/xpagent-badge.svg" -o "$TMP_DIR/xpagent-32.png"
rsvg-convert -w 48 -h 48 "$SVG_DIR/xpagent-badge.svg" -o "$TMP_DIR/xpagent-48.png"
magick "$TMP_DIR/xpagent-16.png" "$TMP_DIR/xpagent-32.png" "$TMP_DIR/xpagent-48.png" "$ICO_DIR/xpagent.ico"

echo "Wrote $BMP_DIR and $ICO_DIR"
