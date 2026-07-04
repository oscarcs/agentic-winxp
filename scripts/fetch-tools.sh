#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="$ROOT/transfer/tools"

TCC_URL="https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip"
WINAPI_URL="https://download.savannah.gnu.org/releases/tinycc/winapi-full-for-0.9.27.zip"

TCC_ZIP="$TOOLS_DIR/tcc-0.9.27-win32-bin.zip"
WINAPI_ZIP="$TOOLS_DIR/winapi-full-for-0.9.27.zip"

TCC_SHA256="02e2bfe8c272a549b15e4bfa4507bd7e05304692af1761db6c1e8e88af675651"
WINAPI_SHA256="8915d4ea9e7e3252a0e54a15e754ec524c4c39746948d5f39fad10c23faa31aa"

download_if_missing() {
  local url="$1"
  local out="$2"

  if [[ -f "$out" ]]; then
    echo "Already have $out"
    return
  fi

  mkdir -p "$(dirname "$out")"
  curl -L --fail --output "$out" "$url"
}

verify_sha256() {
  local expected="$1"
  local file="$2"
  local actual

  actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    echo "Checksum mismatch for $file" >&2
    echo "Expected: $expected" >&2
    echo "Actual:   $actual" >&2
    exit 1
  fi
}

download_if_missing "$TCC_URL" "$TCC_ZIP"
download_if_missing "$WINAPI_URL" "$WINAPI_ZIP"

verify_sha256 "$TCC_SHA256" "$TCC_ZIP"
verify_sha256 "$WINAPI_SHA256" "$WINAPI_ZIP"

echo "TinyCC tools are ready in $TOOLS_DIR"
