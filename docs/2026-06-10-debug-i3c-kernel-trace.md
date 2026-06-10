# 2026-06-10 Debug: I3C Kernel Trace Prints

## Timestamp
2026-06-10

## Objective
Add trace prints to Linux I3C subsystem to clarify probe, initialization, and SDR read/write paths. Rebuild kernel, run verification, capture I3C logs.

## Code Changes

### ast2600-i3c-master.c (drivers/i3c/master/)
- Added `dev_info` prints at probe entry, global-regs mapping, sda-pullup, and before dw_i3c_common_probe call.

### dw-i3c-master.c (drivers/i3c/master/)
- `dw_i3c_common_probe()`: prints for MMIO mapping, core_clk, IRQ number, hw caps (cmd/data FIFO depth, max devs, DAT start), i3c_master_register call, and completion.
- `dw_i3c_master_bus_init()`: prints for bus_mode, master dyn_addr, bus_init completion.
- `dw_i3c_master_daa()`: prints for DAA start (maxdevs, free_pos), ENTDAA command queued, and DAA result (rx_len, newdevs/olddevs).
- `dw_i3c_master_i3c_xfers()`: prints for xfer start (nxfers, mode, dev_index) and result (ret).
- `dw_i3c_master_start_xfer_locked()`: prints for ncmds and per-cmd debug (tx_len, cmd_hi, cmd_lo).
- `dw_i3c_master_end_xfer_locked()`: prints for nresp and per-response (tid, data_len, error).
- `dw_i3c_master_irq_handler()`: prints for IRQ status.
- `dw_i3c_master_send_ccc_cmd()`: prints for ccc_id, ndests, rnw.

### master.c (drivers/i3c/)
- `i3c_master_register()`: prints for start, bus_init id, calling i3c_master_bus_init, init_done.
- `i3c_master_bus_init()`: prints for start, controller bus_init return, rstdaa, do_daa.
- `i3c_master_retrieve_dev_info()`: prints for dyn_addr.
- `i3c_master_add_i3c_dev_locked()`: prints for addr.

### device.c (drivers/i3c/)
- `i3c_device_do_xfers()`: prints for nxfers, mode.

## Build & Test Results

### Kernel Build
- `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j8` succeeded.
- zImage generated OK.

### QEMU Run (30s timeout)
- Kernel boots to shell.
- I3C probe trace appears and confirms full probe flow:
  ```
  ast2600-i3c: probe start, compatible=i3c@1e7a2000
  ast2600-i3c: global-regs idx=0 mapped OK
  ast2600-i3c: sda-pullup=2000 ohms
  ast2600-i3c: calling dw_i3c_common_probe
  dw-i3c: common_probe start
  dw-i3c: MMIO regs mapped OK
  dw-i3c: core_clk enabled
  dw-i3c: irq=55
  dw-i3c: hw caps cmd_fifo=16 data_fifo=16 max_devs=8 dat_start=0x100
  dw-i3c: calling i3c_master_register
  i3c: master_register start, secondary=0
  i3c: bus_init id=0
  i3c: calling i3c_master_bus_init
  i3c: bus_init start, attaching boardinfo devices
  dw-i3c: bus_init start, bus_mode=0
  dw-i3c: master dyn_addr=0x08
  dw-i3c: bus_init complete, controller enabled
  dw-i3c: i3c_master_register OK, probe complete
  ```

### Missing Traces
- `i3c: controller bus_init returned %d` — not captured
- `i3c: rstdaa before DAA` — not captured
- `i3c: do_daa start` — not captured
- `i3c: init_done, registering new i3c devs` — not captured
- dw-i3c DAA/CCC/SDR transfer traces — not captured

### Root Cause Analysis (Unresolved)
The `pr_info` traces added in `master.c` between `ops->bus_init` return and `i3c_master_register OK` do not appear in the console output. Possible causes:
1. The DAA/CCC/transfer flow happens via QEMU MMIO simulation which processes commands asynchronously (IRQ completion), and the timing in QEMU may not match the `timeout 30s` window.
2. The `pr_info` in `master.c` does not carry a device prefix — it may still appear but is interleaved with other kernel output and harder to spot.
3. The verification script (`verify_ast2600_i3c_target.sh`) uses `timeout 20s`, which may be too short for the full DAA + target driver probe + SDR test to complete under QEMU's simulated timing.
4. The initramfs `poweroff` command may not trigger correctly, causing the script to miss the shutdown window.

### Script Result
- `verify_ast2600_i3c_target.sh` exited with "no I3C target devices found".
- `sysfs/bus/i3c/devices` shows `i3c-0` (the master controller) but no target device nodes appear in the captured log window.

## Next Steps
1. Investigate why DAA and SDR trace prints are not appearing — likely a timing/synchronization issue with QEMU's MMIO model and the kernel's wait_for_completion_timeout.
2. Increase QEMU timeout or add explicit delay in initramfs to ensure DAA completes before shell interaction.
3. Consider adding `printk` with `KERN_WARNING` level (more visible) for critical path points.
4. Debug the QEMU aspeed_i3c model's command processing latency to ensure responses arrive within kernel timeout windows.
