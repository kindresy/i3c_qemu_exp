#!/bin/bash
# Verify that the AST2600 QEMU I3C controller exposes synthetic I3C targets.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_BIN="${QEMU:-$ROOT_DIR/qemu-6.2+dfsg/build-arm-softmmu/qemu-system-arm}"
LOG="${LOG:-/tmp/qemu_ast2600_i3c_target_verify.log}"
EXPECT_TARGETS="${EXPECT_TARGETS:-0-123456789abc 0-123456789abd}"
QEMU_I3C_TARGET_COUNT="${QEMU_I3C_TARGET_COUNT:-}"
QEMU_I3C_LATE_TARGET_COUNT="${QEMU_I3C_LATE_TARGET_COUNT:-}"
VERIFY_I3C_LATE_HOTJOIN="${VERIFY_I3C_LATE_HOTJOIN:-0}"

if [ ! -x "$QEMU_BIN" ]; then
    echo "missing executable QEMU: $QEMU_BIN" >&2
    exit 1
fi

rm -f "$LOG"

if [ "$VERIFY_I3C_LATE_HOTJOIN" = "1" ]; then
    (
        sleep 5
        echo 'echo __I3C_INITIAL_TARGETS_BEGIN__'
        echo '/bin/busybox ls /sys/bus/i3c/devices 2>/dev/null || true'
        echo 'echo __I3C_INITIAL_TARGETS_END__'
        echo 'echo __I3C_HOTJOIN_BEGIN__'
        echo 'HOTJOIN=/sys/bus/i3c/devices/i3c-0/hotjoin'
        echo 'if [ ! -e "$HOTJOIN" ]; then HOTJOIN=$(/bin/busybox find -L /sys/bus/i3c/devices -name hotjoin 2>/dev/null | /bin/busybox head -n 1); fi'
        echo 'DO_DAA=/sys/bus/i3c/devices/i3c-0/do_daa'
        echo 'if [ ! -e "$DO_DAA" ]; then DO_DAA=$(/bin/busybox find -L /sys/bus/i3c/devices -name do_daa 2>/dev/null | /bin/busybox head -n 1); fi'
        echo 'echo HOTJOIN_PATH=${HOTJOIN:-missing}'
        echo 'echo DO_DAA_PATH=${DO_DAA:-missing}'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then echo 1 > "$HOTJOIN"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'if [ -n "$DO_DAA" ]; then echo 1 > "$DO_DAA"; fi'
        echo 'sleep 2'
        echo 'echo __I3C_LATE_TARGETS_BEGIN__'
        echo '/bin/busybox ls /sys/bus/i3c/devices 2>/dev/null || true'
        echo 'echo __I3C_LATE_TARGETS_END__'
        echo 'if [ -n "$HOTJOIN" ]; then echo 0 > "$HOTJOIN"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'echo __I3C_HOTJOIN_END__'
        echo '/bin/busybox poweroff -f'
    ) | timeout 25s env QEMU="$QEMU_BIN" LOG="$LOG" "$ROOT_DIR/boot_ast2600_i3c.sh" >/tmp/qemu_ast2600_i3c_target_verify.stdout 2>&1 || true
else
    (
        sleep 5
        echo 'echo __I3C_TARGETS_BEGIN__'
        echo '/bin/busybox ls /sys/bus/i3c/devices 2>/dev/null || true'
        echo 'echo __I3C_TARGETS_END__'
        echo 'echo __I3C_HOTJOIN_BEGIN__'
        echo 'HOTJOIN=/sys/bus/i3c/devices/i3c-0/hotjoin'
        echo 'if [ ! -e "$HOTJOIN" ]; then HOTJOIN=$(/bin/busybox find -L /sys/bus/i3c/devices -name hotjoin 2>/dev/null | /bin/busybox head -n 1); fi'
        echo 'echo HOTJOIN_PATH=${HOTJOIN:-missing}'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then echo 1 > "$HOTJOIN"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then echo 0 > "$HOTJOIN"; fi'
        echo 'if [ -n "$HOTJOIN" ]; then /bin/busybox printf "HJ=%s\n" "$(/bin/busybox cat "$HOTJOIN")"; fi'
        echo 'echo __I3C_HOTJOIN_END__'
        echo '/bin/busybox poweroff -f'
    ) | timeout 20s env QEMU="$QEMU_BIN" LOG="$LOG" "$ROOT_DIR/boot_ast2600_i3c.sh" >/tmp/qemu_ast2600_i3c_target_verify.stdout 2>&1 || true
fi

cat /tmp/qemu_ast2600_i3c_target_verify.stdout >> "$LOG"

if rg -q "Kernel panic|Oops|ast2600-i3c-master .*failed" "$LOG"; then
    echo "I3C boot/probe failed; see $LOG" >&2
    exit 1
fi

if rg -q "i3c-synthetic-target-test .*failed|unexpected" "$LOG"; then
    echo "I3C synthetic target SDR test failed; see $LOG" >&2
    exit 1
fi

if [ "$VERIFY_I3C_LATE_HOTJOIN" = "1" ]; then
    initial_targets="$(perl -ne '
        s/\e\[[0-9;]*m//g;
        if (/__I3C_INITIAL_TARGETS_BEGIN__/) { $in = 1; next; }
        if (/__I3C_INITIAL_TARGETS_END__/) { $in = 0; next; }
        print if $in;
    ' "$LOG" | rg -o '[0-9]+-[0-9a-fA-F]{12,}' | sort -u || true)"
    if ! grep -qx "0-123456789abc" <<<"$initial_targets"; then
        echo "missing initial I3C target 0-123456789abc; found: $initial_targets; see $LOG" >&2
        exit 1
    fi
    if grep -qx "0-123456789abd" <<<"$initial_targets"; then
        echo "late I3C target appeared before hotjoin DAA; found: $initial_targets; see $LOG" >&2
        exit 1
    fi

    targets="$(perl -ne '
        s/\e\[[0-9;]*m//g;
        if (/__I3C_LATE_TARGETS_BEGIN__/) { $in = 1; next; }
        if (/__I3C_LATE_TARGETS_END__/) { $in = 0; next; }
        print if $in;
    ' "$LOG" | rg -o '[0-9]+-[0-9a-fA-F]{12,}' | sort -u || true)"
else
    targets="$(perl -pe 's/\e\[[0-9;]*m//g' "$LOG" \
        | rg -o '[0-9]+-[0-9a-fA-F]{12,}' \
        | sort -u || true)"
fi
if [ -z "$targets" ]; then
    echo "no I3C target devices found; see $LOG" >&2
    exit 1
fi

for target in $EXPECT_TARGETS; do
    if ! grep -qx "$target" <<<"$targets"; then
        echo "missing expected I3C target $target; found: $targets; see $LOG" >&2
        exit 1
    fi
done

expected_count="$(wc -w <<<"$EXPECT_TARGETS")"
actual_count="$(wc -l <<<"$targets")"
if [ "$actual_count" -ne "$expected_count" ]; then
    echo "unexpected I3C target count $actual_count; expected $expected_count; found: $targets; see $LOG" >&2
    exit 1
fi

if ! rg -q "i3c-synthetic-target-test .*private SDR read/write OK reg=0x10 value=0x5a" "$LOG"; then
    echo "I3C synthetic target SDR test did not pass; see $LOG" >&2
    exit 1
fi

if ! rg -q "i3c-synthetic-target-test .*malformed private SDR write rejected" "$LOG"; then
    echo "I3C malformed private SDR test did not pass; see $LOG" >&2
    exit 1
fi

hotjoin_states="$(perl -ne '
    s/\e\[[0-9;]*m//g;
    if (/__I3C_HOTJOIN_BEGIN__/) { $in = 1; next; }
    if (/__I3C_HOTJOIN_END__/) { $in = 0; next; }
    print "$1\n" if $in && /HJ=([01])/;
' "$LOG" | tr "\n" " ")"
case "$hotjoin_states" in
*"0 1 0 "*) ;;
*)
    echo "I3C hotjoin sysfs toggle failed; states: ${hotjoin_states:-none}; see $LOG" >&2
    exit 1
    ;;
esac

echo "$targets"
