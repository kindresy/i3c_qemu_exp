# I3C Late Hotjoin, IBI, and Documentation Plan

## Scope
Implement the remaining I3C work in the requested order:

1. Late target / hotjoin attach path.
2. IBI model and Linux validation.
3. Patch series and teaching documentation.

Skip PID/BCR/DCR identity-value verification for now.

## Files
- `qemu-mods/hw/misc/aspeed_i3c.c`: source-of-record QEMU I3C model changes.
- `qemu-mods/include/hw/misc/aspeed_i3c.h`: source-of-record QEMU I3C state fields.
- `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c`: local build tree mirror used by verification.
- `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h`: local build tree mirror used by verification.
- `boot_ast2600_i3c.sh`: QEMU property plumbing.
- `scripts/verify_ast2600_i3c_target.sh`: regression and late hotjoin validation.
- `linux/drivers/i3c/i3c-synthetic-target-test.c`: later IBI request/handler validation.
- `patches/linux/0004-i3c-add-synthetic-target-test-source.patch`: Linux source patch sync.
- `docs/`: progress records and final teaching material.

## Step 1: Late Target RED
Add a verification mode:

```sh
VERIFY_I3C_LATE_HOTJOIN=1 \
QEMU_I3C_TARGET_COUNT=1 \
QEMU_I3C_LATE_TARGET_COUNT=1 \
EXPECT_TARGETS="0-123456789abc 0-123456789abd" \
LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_verify.log \
scripts/verify_ast2600_i3c_target.sh
```

Expected RED before implementation: final target list still contains only `0-123456789abc`.

## Step 2: Late Target GREEN
In QEMU:
- Keep `synth-target-count` as the initially visible target count.
- Add `synth-late-target-count`.
- Add runtime `late_targets_visible`.
- On guest write to `DEVICE_CTRL` that clears `HOT_JOIN_NACK`, set `late_targets_visible = true`.
- Make DAA search unassigned targets up to `initial + late` only after late targets are visible.

In the script:
- Before hotjoin, require only `0-123456789abc`.
- Enable hotjoin by writing `1` to sysfs.
- Trigger `do_daa` by writing `1` to sysfs.
- After DAA, require `0-123456789abc` and `0-123456789abd`.

## Step 3: IBI
After late target attach is passing:
- Extend the synthetic Linux driver to call `i3c_device_request_ibi()` and `i3c_device_enable_ibi()`.
- Extend QEMU with a minimal DW-compatible IBI status/data queue.
- Add a script assertion for a driver log line showing the IBI handler received the expected payload.

## Step 4: Patch Series and Docs
- Generate or sync QEMU and Linux patch files.
- Update the I3C Basic Spec article with a practical workflow section covering DAA, private SDR, hotjoin/late attach, and IBI.
- Keep all original article content intact.

## Verification Gate
Before each commit:

```sh
bash -n boot_ast2600_i3c.sh
bash -n scripts/verify_ast2600_i3c_target.sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
scripts/verify_ast2600_i3c_target.sh
```

For late hotjoin:

```sh
VERIFY_I3C_LATE_HOTJOIN=1 QEMU_I3C_TARGET_COUNT=1 QEMU_I3C_LATE_TARGET_COUNT=1 EXPECT_TARGETS="0-123456789abc 0-123456789abd" LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_verify.log scripts/verify_ast2600_i3c_target.sh
```
