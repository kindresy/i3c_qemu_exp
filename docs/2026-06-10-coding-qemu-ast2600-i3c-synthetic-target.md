# 2026-06-10-coding-qemu-ast2600-i3c-synthetic-target

## Timestamp
2026-06-10 CST

## Objective
在已有 AST2600 I3C minimal controller stub 基础上，模拟一个 synthetic I3C target，让 Linux I3C core 通过 DAA 枚举出真实 target 设备节点。

## Commands Used / Code Changes

Commands:

```bash
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
scripts/verify_ast2600_i3c_target.sh
rg -n "Kernel panic|Oops|ast2600-i3c-master .*failed|aspeed_i3c:" /tmp/qemu_ast2600_i3c_target_verify.log
rg -n "0-123456789abc|i3c-0" /tmp/qemu_ast2600_i3c_target_verify.log
bash -n boot_ast2600_i3c.sh scripts/verify_ast2600_i3c_target.sh
```

Code changes:

- Extended `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c` with one synthetic target.
- Extended `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h` with target and RX FIFO state.
- Added `scripts/verify_ast2600_i3c_target.sh` as a host-side regression check.

## Implementation Notes

The synthetic target has fixed identity:

- PID: `0x123456789abc`
- BCR: `0x00`
- DCR: `0x42`

The model implements only the path Linux currently exercises for enumeration:

- DAA command allocates the first Linux-prepared DAT dynamic address.
- The response uses `data_len = I3C_DAT_DEPTH - dev_index - 1`, which makes the DW driver mark exactly one newly discovered slot.
- The DAA response also reports `IBA_NACK` to avoid the current DW IRQ handler reading DAA payload into a NULL RX buffer. The DW `do_daa()` path still uses `cmd->rx_len` for discovered-device accounting.
- Direct CCC reads for `GETPID`, `GETBCR`, and `GETDCR` return data through the emulated RX FIFO.
- Other CCC reads currently return zero-length success unless future tests require stricter behavior.

This is still not a full I3C bus implementation. It is one synthetic target inside the AST2600 controller model.

## Results or Observations

- `ninja -C build-arm-softmmu qemu-system-arm` passes.
- `scripts/verify_ast2600_i3c_target.sh` passes and prints:

```text
0-123456789abc
```

- Final boot log contains the sysfs entries:

```text
0-123456789abc  i3c-0
```

- Final boot log has no `Kernel panic`, `Oops`, `ast2600-i3c-master ... failed`, or leftover temporary `aspeed_i3c:` debug output.

## Next Steps

- Add simple private SDR read/write only if a real consumer driver or userspace test requires it.
- If multiple targets are needed, generalize the current target state into a small fixed target table before attempting a QEMU-wide I3C bus abstraction.
