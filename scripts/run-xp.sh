#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DISK="${WINXP_DISK:-$ROOT/vm/winxp.qcow2}"
QEMU="${QEMU:-qemu-system-i386}"
RAM="${WINXP_RAM:-512}"
EXTRA_CD="${WINXP_CDROM:-}"
RESIZE="${WINXP_RESIZE:-1}"

usage() {
  cat <<EOF
Usage: $0 [--resize|--no-resize]

Options:
  --resize      Scale the XP display to fit the Cocoa window. This is default.
  --no-resize   Keep the QEMU Cocoa window at the guest display size.

Environment:
  WINXP_RAM     RAM in MB. Default: 512
  WINXP_CDROM   Optional ISO to mount as CD-ROM
  WINXP_RESIZE  1/0, on/off, true/false. Default: 1
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --resize|--resizable)
      RESIZE=1
      ;;
    --no-resize|--fixed-size)
      RESIZE=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
  shift
done

if [[ ! -f "$DISK" ]]; then
  echo "Missing VM disk: $DISK" >&2
  echo "Create it with: qemu-img create -f qcow2 \"$DISK\" 16G" >&2
  exit 1
fi

if [[ -n "$EXTRA_CD" ]]; then
  if [[ ! -f "$EXTRA_CD" ]]; then
    echo "Missing CD-ROM image: $EXTRA_CD" >&2
    exit 1
  fi
fi

case "$RESIZE" in
  0|false|FALSE|off|OFF|no|NO)
    DISPLAY_BACKEND="cocoa,zoom-to-fit=off"
    ;;
  *)
    DISPLAY_BACKEND="cocoa,zoom-to-fit=on"
    ;;
esac

set -- "$QEMU" \
  -name "Windows XP SP3" \
  -accel tcg \
  -M pc \
  -cpu pentium3 \
  -smp 1 \
  -m "$RAM" \
  -rtc base=localtime,clock=host \
  -drive "file=$DISK,format=qcow2,if=ide,index=0,media=disk"

if [[ -n "$EXTRA_CD" ]]; then
  set -- "$@" -drive "file=$EXTRA_CD,media=cdrom,if=ide,index=2,readonly=on"
fi

set -- "$@" \
  -boot order=c,menu=on \
  -vga cirrus \
  -usb \
  -device usb-tablet \
  -netdev user,id=net0 \
  -device rtl8139,netdev=net0 \
  -display "$DISPLAY_BACKEND"

exec "$@"
