#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="${WINXP_ISO:-$ROOT/en_windows_xp_professional_with_service_pack_3_x86_cd_vl_x14-73974.iso}"
DISK="${WINXP_DISK:-$ROOT/vm/winxp.qcow2}"
QEMU="${QEMU:-qemu-system-i386}"
RAM="${WINXP_RAM:-512}"

if [[ ! -f "$ISO" ]]; then
  echo "Missing XP ISO: $ISO" >&2
  exit 1
fi

if [[ ! -f "$DISK" ]]; then
  echo "Missing VM disk: $DISK" >&2
  echo "Create it with: qemu-img create -f qcow2 \"$DISK\" 16G" >&2
  exit 1
fi

exec "$QEMU" \
  -name "Windows XP SP3" \
  -accel tcg \
  -M pc \
  -cpu pentium3 \
  -smp 1 \
  -m "$RAM" \
  -rtc base=localtime,clock=host \
  -drive "file=$DISK,format=qcow2,if=ide,index=0,media=disk" \
  -drive "file=$ISO,media=cdrom,if=ide,index=2,readonly=on" \
  -boot order=c,once=d,menu=on \
  -vga cirrus \
  -usb \
  -device usb-tablet \
  -netdev user,id=net0 \
  -device rtl8139,netdev=net0 \
  -display cocoa
