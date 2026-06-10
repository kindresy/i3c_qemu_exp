# 2026-06-10 Coding: QEMU AST2600 I3C Late Target Hotjoin

## Timestamp
2026-06-10 CST

## Category
Coding

## Objective
Add a runtime attach flow for the AST2600 QEMU I3C model so a target can appear after boot and be discovered by a second Linux DAA pass.

## Changes
- Added `synth-late-target-count` to the ASPEED I3C QOM properties.
- Kept `synth-target-count` as the initially visible synthetic target count.
- Added runtime `late_targets_visible` state.
- When Linux enables hotjoin by clearing `DEV_CTRL_HOT_JOIN_NACK`, QEMU marks late targets visible.
- The next ENTDAA assigns the newly visible target.
- Extended `boot_ast2600_i3c.sh` with `QEMU_I3C_LATE_TARGET_COUNT`.
- Extended `scripts/verify_ast2600_i3c_target.sh` with `VERIFY_I3C_LATE_HOTJOIN=1`.

## RED
Before QEMU implemented the new property, the late hotjoin verification failed at startup:

```text
qemu-system-arm: can't apply global aspeed.i3c-ast2600.synth-late-target-count=1: Property 'aspeed.i3c-ast2600.synth-late-target-count' not found
```

## Commands
Built QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Ran late hotjoin verification:

```sh
VERIFY_I3C_LATE_HOTJOIN=1 QEMU_I3C_TARGET_COUNT=1 QEMU_I3C_LATE_TARGET_COUNT=1 EXPECT_TARGETS="0-123456789abc 0-123456789abd" LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_verify.log scripts/verify_ast2600_i3c_target.sh
```

Ran default regression:

```sh
scripts/verify_ast2600_i3c_target.sh
```

## Verification Result
Late hotjoin output:

```text
0-123456789abc
0-123456789abd
```

Default output:

```text
0-123456789abc
0-123456789abd
```

Relevant late hotjoin log path:

```text
/tmp/qemu_ast2600_i3c_late_hotjoin_verify.log
```

## Result
The model now supports a minimal runtime attach workflow:

1. Boot with one visible I3C target.
2. Enable hotjoin through Linux sysfs.
3. Trigger `do_daa` through Linux sysfs.
4. Discover the late synthetic target.

This validates the Linux DAA re-entry path for a target that was not present during initial bus initialization.

## Next Steps
- Add a true IBI path using the DW IBI queue and the synthetic Linux target driver.
- Later, model automatic hotjoin IBI event delivery instead of manually triggering `do_daa`.
