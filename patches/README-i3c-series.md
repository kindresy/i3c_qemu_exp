# I3C Patch Series

## Purpose
This directory preserves the patch-level recipe for reproducing the AST2600 QEMU + Linux I3C workflow from this branch.

The source-of-record files in this repo are:

- QEMU I3C model: `qemu-mods/hw/misc/aspeed_i3c.c`
- QEMU I3C header: `qemu-mods/include/hw/misc/aspeed_i3c.h`
- Linux synthetic target source patch: `patches/linux/0004-i3c-add-synthetic-target-test-source.patch`
- Verification script: `scripts/verify_ast2600_i3c_target.sh`

## QEMU Series
Apply to QEMU 6.2 source:

```sh
git am patches/qemu/0001-misc-add-aspeed-i3c-device-model.patch
git am patches/qemu/0002-arm-aspeed-wire-i3c-into-ast2600-soc.patch
```

`0001` is documented as a compact source patch. For exact current source, copy the source-of-record files:

```sh
cp qemu-mods/hw/misc/aspeed_i3c.c qemu-6.2+dfsg/hw/misc/aspeed_i3c.c
cp qemu-mods/include/hw/misc/aspeed_i3c.h qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h
```

Then build:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

## Linux Series
Apply to Linux source:

```sh
git am patches/linux/0001-dts-aspeed-g6-add-i3c-node.patch
git am patches/linux/0002-i3c-add-synthetic-target-test-Kconfig.patch
git am patches/linux/0003-i3c-add-synthetic-target-test-driver.patch
git am patches/linux/0004-i3c-add-synthetic-target-test-source.patch
git am patches/linux/0005-i3c-add-workflow-trace-prints.patch
```

Then build:

```sh
make -C linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage aspeed/aspeed-ast2600-evb.dtb
```

## Verification
Default two-target, SDR, malformed write, hotjoin sysfs, and IBI:

```sh
scripts/verify_ast2600_i3c_target.sh
```

Late hotjoin plus IBI:

```sh
VERIFY_I3C_LATE_HOTJOIN=1 \
QEMU_I3C_TARGET_COUNT=1 \
QEMU_I3C_LATE_TARGET_COUNT=1 \
EXPECT_TARGETS="0-123456789abc 0-123456789abd" \
LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_ibi_verify.log \
scripts/verify_ast2600_i3c_target.sh
```

## Current Coverage
- AST2600 DesignWare-compatible I3C master MMIO model
- DAA with configurable synthetic targets
- direct CCC `GETPID`, `GETBCR`, `GETDCR`
- private SDR read/write
- malformed private SDR rejection
- hotjoin sysfs enable/disable
- late target discovery through a second DAA pass
- minimal SIR IBI delivery into Linux handler
