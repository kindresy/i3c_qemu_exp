# 2026-06-10 Coding: I3C Malformed Private SDR and Hotjoin Control Check

## Timestamp
2026-06-10 CST

## Category
Coding

## Objective
Extend the AST2600 I3C validation flow with:

- a malformed private SDR transfer negative test
- a hotjoin sysfs control-plane toggle check

## Changes
- Updated the QEMU AST2600 I3C model to reject a standalone 1-byte private SDR write.
  - A 1-byte write without `TOC` is still accepted as a register-pointer phase before a read.
  - A 1-byte write with `TOC` is treated as malformed and returns `IBA_NACK`.
- Updated the Linux synthetic target test driver to verify that malformed write is rejected.
- Updated `scripts/verify_ast2600_i3c_target.sh` to require:
  - two enumerated synthetic targets
  - private SDR read/write success
  - malformed private SDR write rejection
  - hotjoin sysfs state transition `0 -> 1 -> 0`
- Synced the Linux source patch in `patches/linux/0004-i3c-add-synthetic-target-test-source.patch`.

## Commands
Built QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Built Linux:

```sh
make -C linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage aspeed/aspeed-ast2600-evb.dtb
```

Ran verification:

```sh
bash -n scripts/verify_ast2600_i3c_target.sh
scripts/verify_ast2600_i3c_target.sh
```

## Verification Result
Observed script output:

```text
0-123456789abc
0-123456789abd
```

Relevant log lines from `/tmp/qemu_ast2600_i3c_target_verify.log`:

```text
dw-i3c: DAA result rx_len=6 newdevs=0x3 olddevs=0xffffff00
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
i3c-synthetic-target-test 0-123456789abc: malformed private SDR write rejected ret=-5
HOTJOIN_PATH=/sys/bus/i3c/devices/i3c-0/hotjoin
HJ=0
HJ=1
HJ=0
```

## Result
The validation flow now covers both the positive private SDR data path and a negative malformed private SDR path. It also proves that the Linux DW I3C hotjoin control-plane sysfs switch can toggle under the AST2600 QEMU model.

## Next Steps
- If we want actual hotjoin event handling, add a new late synthetic target and model the IBI/hotjoin interrupt queue path.
- If we want IBI validation, wire a synthetic target driver to `i3c_device_request_ibi()` and implement the DW IBI queue behavior in QEMU.
