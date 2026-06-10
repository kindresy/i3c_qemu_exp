#!/bin/bash
# QEMU AST2600 EVB boot script for the ASPEED I3C route.

set -euo pipefail

KERNEL="${KERNEL:-linux/arch/arm/boot/zImage}"
DTB="${DTB:-linux/arch/arm/boot/dts/aspeed/aspeed-ast2600-evb.dtb}"
INITRD="${INITRD:-ast2600_initramfs.cpio.gz}"
LOG="${LOG:-/tmp/qemu_ast2600_i3c.log}"
APPEND="${APPEND:-console=ttyS4,115200n8 root=/dev/ram rw loglevel=8}"
QEMU="${QEMU:-qemu-system-arm}"
QEMU_EXTRA_ARGS="${QEMU_EXTRA_ARGS:-}"
QEMU_I3C_TARGET_COUNT="${QEMU_I3C_TARGET_COUNT:-}"
QEMU_I3C_LATE_TARGET_COUNT="${QEMU_I3C_LATE_TARGET_COUNT:-}"

for file in "$KERNEL" "$DTB" "$INITRD"; do
    if [ ! -f "$file" ]; then
        echo "missing required boot artifact: $file" >&2
        exit 1
    fi
done

echo "Starting QEMU AST2600 EVB with I3C-enabled kernel/device tree..."
echo "Kernel: $KERNEL"
echo "DTB: $DTB"
echo "Initrd: $INITRD"
echo "QEMU: $QEMU"
echo "Log: $LOG"

if [ -n "$QEMU_I3C_TARGET_COUNT" ]; then
    QEMU_EXTRA_ARGS="$QEMU_EXTRA_ARGS -global driver=aspeed.i3c-ast2600,property=synth-target-count,value=$QEMU_I3C_TARGET_COUNT"
fi
if [ -n "$QEMU_I3C_LATE_TARGET_COUNT" ]; then
    QEMU_EXTRA_ARGS="$QEMU_EXTRA_ARGS -global driver=aspeed.i3c-ast2600,property=synth-late-target-count,value=$QEMU_I3C_LATE_TARGET_COUNT"
fi

"$QEMU" \
    -M ast2600-evb \
    -kernel "$KERNEL" \
    -dtb "$DTB" \
    -initrd "$INITRD" \
    -append "$APPEND" \
    -nographic \
    -no-reboot \
    $QEMU_EXTRA_ARGS \
    2>&1 | tee "$LOG"
