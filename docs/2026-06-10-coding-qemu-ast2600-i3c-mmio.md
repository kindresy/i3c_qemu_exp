# 2026-06-10-coding-qemu-ast2600-i3c-mmio

## Timestamp
2026-06-10 CST

## Objective
在 ASPEED/AST2600 QEMU 机器路线下，为本地 QEMU 6.2 源码补一个最小 AST2600 I3C MMIO 模型，使 Linux 内置 `ast2600-i3c-master` 不再因 QEMU 缺失 I3C 控制器行为而 probe 超时。

## Commands Used / Code Changes

Commands:

```bash
apt source qemu
python3 -m pip install --target /home/luyuan/qemu_exp/.tools/meson meson==0.61.2
../configure --target-list=arm-softmmu --disable-docs --disable-werror --meson=/home/luyuan/qemu_exp/.tools/meson/meson-local
ninja -C build-arm-softmmu qemu-system-arm
timeout 20s env QEMU=/home/luyuan/qemu_exp/qemu-6.2+dfsg/build-arm-softmmu/qemu-system-arm ./boot_ast2600_i3c.sh
rg -n "ast2600-i3c|i3c|Kernel panic|Oops|failed with error -110" /tmp/qemu_ast2600_i3c.log
bash -n boot_ast2600_i3c.sh
```

Code changes:

- Added `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h`.
- Added `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c`.
- Added `aspeed_i3c.c` to `qemu-6.2+dfsg/hw/misc/meson.build`.
- Added `AspeedI3CState i3c` and `ASPEED_DEV_I3C0` to `qemu-6.2+dfsg/include/hw/arm/aspeed_soc.h`.
- Wired AST2600 I3C0 at `0x1e7a2000` with IRQ map value `102` in `qemu-6.2+dfsg/hw/arm/aspeed_ast2600.c`.
- Updated `boot_ast2600_i3c.sh` to allow `QEMU=...` override.
- Added local Meson wrapper files under `.tools/meson/` because system Meson was not installed and passwordless `sudo apt-get install` was not available.

## Implementation Notes

The QEMU device is intentionally a minimal controller model, not a full I3C bus model:

- It exposes DesignWare-style MMIO registers used by `linux/drivers/i3c/master/dw-i3c-master.c`.
- It returns plausible FIFO/DAT capabilities through `QUEUE_STATUS_LEVEL`, `DATA_BUFFER_STATUS_LEVEL`, and `DEVICE_ADDR_TABLE_POINTER`.
- It generates response queue entries when Linux writes command pairs to `COMMAND_QUEUE_PORT`.
- It raises IRQ through `INTR_RESP_READY_STAT` when response entries are available.
- For `ENTDAA` on an empty emulated bus, it returns `IBA_NACK` plus `data_len = maxdevs`; this avoids fake device registration and avoids RX FIFO reads into a NULL DAA buffer in the current DW driver path.

## Results or Observations

- `ninja -C build-arm-softmmu qemu-system-arm` passes and links `/home/luyuan/qemu_exp/qemu-6.2+dfsg/build-arm-softmmu/qemu-system-arm`.
- Booting with the rebuilt QEMU reaches `/init` and drops to shell.
- The old I3C failure `ast2600-i3c-master 1e7a2000.i3c: probe with driver ast2600-i3c-master failed with error -110` is gone.
- `/tmp/qemu_ast2600_i3c.log` contains no I3C oops or panic after the final run.
- The remaining `failed with error -110` in the final log is from `fsi-master-aspeed 1e79b000.fsi`, unrelated to the I3C route.
- The boot command exits with `124` only because `timeout 20s` kills the interactive shell after successful boot.

## Next Steps

- If the next goal is to emulate real I3C targets, extend the model beyond the current empty-bus command completion stub.
- If the current goal is only AST2600 I3C driver probe/boot validation, use:

```bash
QEMU=/home/luyuan/qemu_exp/qemu-6.2+dfsg/build-arm-softmmu/qemu-system-arm ./boot_ast2600_i3c.sh
```
