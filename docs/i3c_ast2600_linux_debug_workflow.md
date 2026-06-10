# AST2600 QEMU + Linux I3C Debug Workflow

## Current Branch
`debug/i3c-workflow-trace`

Latest validated stages:

- configurable synthetic targets
- late hotjoin target discovery
- SIR IBI delivery

## Build
QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Linux:

```sh
make -C linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage aspeed/aspeed-ast2600-evb.dtb
```

## Default Verification
```sh
scripts/verify_ast2600_i3c_target.sh
```

Expected output:

```text
0-123456789abc
0-123456789abd
```

Default log:

```text
/tmp/qemu_ast2600_i3c_target_verify.log
```

Key log lines:

```text
dw-i3c: DAA result rx_len=6 newdevs=0x3 olddevs=0xffffff00
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
i3c-synthetic-target-test 0-123456789abc: malformed private SDR write rejected ret=-5
i3c-synthetic-target-test 0-123456789abc: IBI received len=1 payload=0x7c
HOTJOIN_PATH=/sys/bus/i3c/devices/i3c-0/hotjoin
HJ=0
HJ=1
HJ=0
```

## Late Hotjoin Verification
```sh
VERIFY_I3C_LATE_HOTJOIN=1 \
QEMU_I3C_TARGET_COUNT=1 \
QEMU_I3C_LATE_TARGET_COUNT=1 \
EXPECT_TARGETS="0-123456789abc 0-123456789abd" \
LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_ibi_verify.log \
scripts/verify_ast2600_i3c_target.sh
```

Workflow:

1. Boot with only `0-123456789abc` visible.
2. Enable Linux hotjoin sysfs.
3. QEMU marks late targets visible when `DEV_CTRL_HOT_JOIN_NACK` is cleared.
4. Write `1` to Linux `do_daa`.
5. Linux registers `0-123456789abd`.

## QEMU Model Notes
Important source paths:

- `qemu-mods/hw/misc/aspeed_i3c.c`
- `qemu-mods/include/hw/misc/aspeed_i3c.h`

Core model pieces:

- `aspeed_i3c_assign_targets()` handles ENTDAA.
- `aspeed_i3c_prepare_ccc_read()` returns PID/BCR/DCR.
- `aspeed_i3c_apply_private_write()` rejects malformed one-byte writes with `TOC`.
- `aspeed_i3c_queue_ibi()` creates a DW-compatible IBI status entry.
- `aspeed_i3c_read_ibi_queue()` returns the IBI status word first, then payload bytes.

## Linux Driver Notes
Important source path:

- `linux/drivers/i3c/i3c-synthetic-target-test.c`

Probe flow:

1. Read reset register `0x10`, expect `0xa5`.
2. Write `0x5a` to register `0x10`.
3. Read back `0x5a`.
4. Submit malformed one-byte write and expect rejection.
5. Request and enable IBI.
6. IBI handler expects one byte: `0x7c`.

## Debugging Checklist
- If probe times out, check QEMU response IRQ: `INTR_RESP_READY_STAT`.
- If DAA registers no devices, check DAT writes and ENTDAA `rx_len`.
- If SDR read is wrong, check the write-as-register-pointer phase before read.
- If malformed write is accepted, check `TOC` handling on one-byte writes.
- If hotjoin does not expose the late target, check `DEV_CTRL_HOT_JOIN_NACK` transition and `do_daa`.
- If IBI does not arrive, check direct ENEC `ccc_id=0x80`, `QUEUE_STATUS_LEVEL` IBI count, and IRQ `0x14`.
