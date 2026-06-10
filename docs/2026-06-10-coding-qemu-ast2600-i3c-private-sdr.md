# AST2600 QEMU I3C synthetic target private SDR validation

## Goal

Continue the AST2600 QEMU route by making the synthetic I3C target behave more
like a real target for simple private SDR transfers, not just DAA and CCC reads.

The validation target is intentionally small:

- one synthetic I3C device with PID `0x123456789abc`
- a 256-byte register window inside QEMU
- reset value `target_regs[0x10] = 0xa5`
- private SDR write updates register `0x10` to `0x5a`
- private SDR read returns the updated value

## QEMU changes

Files:

- `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c`
- `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h`

Implemented:

- TX FIFO capture from `I3C_RX_TX_DATA_PORT`
- target register window storage
- private SDR write semantics:
  - first payload byte is the target register pointer
  - following bytes are written sequentially
- private SDR read semantics:
  - read starts at the current target register pointer
  - pointer auto-increments
- CCC/private command separation using `COMMAND_PORT_CP`
- migration state coverage for the new TX FIFO and target register fields

Existing DAA and direct CCC reads remain intact:

- ENTDAA
- GETPID
- GETBCR
- GETDCR

## Linux validation driver

Files:

- `linux/drivers/i3c/i3c-synthetic-target-test.c`
- `linux/drivers/i3c/Kconfig`
- `linux/drivers/i3c/Makefile`
- `linux/.config`

Added a built-in test driver behind:

```text
CONFIG_I3C_SYNTHETIC_TARGET_TEST=y
```

The driver binds to the synthetic target PID and performs this probe-time
sequence:

1. private SDR read register `0x10`, expect `0xa5`
2. private SDR write register `0x10 = 0x5a`
3. private SDR read register `0x10`, expect `0x5a`

Success log:

```text
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
```

This is a qemu_exp validation driver, not a production I3C target protocol
driver.

## Verification script

File:

- `scripts/verify_ast2600_i3c_target.sh`

The script now checks both:

- sysfs target enumeration: `0-123456789abc`
- kernel log success from the private SDR validation driver

It fails on:

- `Kernel panic`
- `Oops`
- AST2600 I3C master probe failure
- synthetic target SDR test failure or unexpected value
- missing private SDR success log

## Commands run

Build QEMU:

```sh
ninja -C build-arm-softmmu qemu-system-arm
```

Build Linux zImage and AST2600 EVB DTB:

```sh
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage aspeed/aspeed-ast2600-evb.dtb
```

Run end-to-end verification:

```sh
scripts/verify_ast2600_i3c_target.sh
```

Observed output:

```text
0-123456789abc
```

Observed log evidence:

```text
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
```

## Result

The AST2600 QEMU machine now reaches a practical next step beyond enumeration:
Linux can bind an I3C device driver to the synthetic target and complete a
private SDR read/write/read transaction through the AST2600 I3C master path.

This is still not a full electrical/protocol-accurate I3C target model. It is a
controller-facing behavioral model sufficient for kernel bring-up and basic I3C
target driver validation.
