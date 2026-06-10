#!/bin/bash
# Verify that the AST2600 QEMU I3C controller exposes at least one I3C target.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_BIN="${QEMU:-$ROOT_DIR/qemu-6.2+dfsg/build-arm-softmmu/qemu-system-arm}"
LOG="${LOG:-/tmp/qemu_ast2600_i3c_target_verify.log}"

if [ ! -x "$QEMU_BIN" ]; then
    echo "missing executable QEMU: $QEMU_BIN" >&2
    exit 1
fi

rm -f "$LOG"

(
    sleep 5
    echo 'echo __I3C_TARGETS_BEGIN__'
    echo '/bin/busybox ls /sys/bus/i3c/devices 2>/dev/null || true'
    echo 'echo __I3C_TARGETS_END__'
    echo '/bin/busybox poweroff -f'
) | timeout 20s env QEMU="$QEMU_BIN" LOG="$LOG" "$ROOT_DIR/boot_ast2600_i3c.sh" >/tmp/qemu_ast2600_i3c_target_verify.stdout 2>&1 || true

cat /tmp/qemu_ast2600_i3c_target_verify.stdout >> "$LOG"

if rg -q "Kernel panic|Oops|ast2600-i3c-master .*failed" "$LOG"; then
    echo "I3C boot/probe failed; see $LOG" >&2
    exit 1
fi

if rg -q "i3c-synthetic-target-test .*failed|unexpected" "$LOG"; then
    echo "I3C synthetic target SDR test failed; see $LOG" >&2
    exit 1
fi

targets="$(perl -pe 's/\e\[[0-9;]*m//g' "$LOG" \
    | rg -o '[0-9]+-[0-9a-fA-F]{12,}' \
    | sort -u || true)"
if [ -z "$targets" ]; then
    echo "no I3C target devices found; see $LOG" >&2
    exit 1
fi

if ! rg -q "i3c-synthetic-target-test .*private SDR read/write OK reg=0x10 value=0x5a" "$LOG"; then
    echo "I3C synthetic target SDR test did not pass; see $LOG" >&2
    exit 1
fi

echo "$targets"
