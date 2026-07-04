#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <folder> [output.iso]" >&2
  exit 1
fi

SOURCE_DIR="$1"
OUTPUT="${2:-transfer.iso}"

if [[ ! -d "$SOURCE_DIR" ]]; then
  echo "Missing folder: $SOURCE_DIR" >&2
  exit 1
fi

hdiutil makehybrid -iso -joliet -o "$OUTPUT" "$SOURCE_DIR"
