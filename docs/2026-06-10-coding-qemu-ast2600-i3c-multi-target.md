# 2026-06-10 Coding: AST2600 QEMU I3C Multi-Target Enumeration

## Timestamp
2026-06-10 CST

## Category
Coding

## Objective
Extend the AST2600 QEMU I3C behavioral model from one synthetic target to two synthetic targets, while keeping the existing private SDR validation path working.

## Files Changed
- `qemu-mods/hw/misc/aspeed_i3c.c`
- `qemu-mods/include/hw/misc/aspeed_i3c.h`
- `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c`
- `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h`
- `scripts/verify_ast2600_i3c_target.sh`

## Implementation
- Added `ASPEED_I3C_SYNTH_TARGET_COUNT=2`.
- Converted single-target state into per-target arrays:
  - register windows
  - register pointers
  - assignment state
  - dynamic addresses
  - DAT indexes
- Added target lookup by DesignWare `DEV_INDEX`.
- Updated ENTDAA handling to assign all unassigned synthetic targets into sequential DAT slots in one DAA round.
- Kept the original validation target PID `0x123456789abc`.
- Added a second synthetic target PID `0x123456789abd`.
- Kept private SDR read/write validation on the first target through the existing Linux test driver.

## TDD Check
First updated `scripts/verify_ast2600_i3c_target.sh` to require both:

```text
0-123456789abc
0-123456789abd
```

Before the model change, the script failed as expected:

```text
missing expected I3C target 0-123456789abd; found: 0-123456789abc; see /tmp/qemu_ast2600_i3c_target_verify.log
```

## Verification
Built QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Ran end-to-end verification:

```sh
scripts/verify_ast2600_i3c_target.sh
```

Observed output:

```text
0-123456789abc
0-123456789abd
```

Relevant log evidence from `/tmp/qemu_ast2600_i3c_target_verify.log`:

```text
ast2600-i3c-master 1e7a2000.i3c: dw-i3c: DAA result rx_len=6 newdevs=0x3 olddevs=0xffffff00
i3c: add_i3c_dev_locked addr=0x09
i3c: add_i3c_dev_locked addr=0x0a
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
```

## Result
The AST2600 QEMU I3C model now validates Linux multi-device DAA enumeration and keeps the existing private SDR data-path test passing.

## Next Steps
- Make synthetic target PID/BCR/DCR/register defaults configurable.
- Add negative/error-path tests for unsupported CCC and malformed private SDR transfers.
