# 2026-06-10 Coding: Fix I3C Bus Init Early Exit

## Timestamp
2026-06-10 CST

## Objective
Fix the current AST2600 I3C verification blockers where Linux exits before DAA, leaving no synthetic target in sysfs.

## Commands Used / Code Changes
- Reproduced failure with:

```sh
scripts/verify_ast2600_i3c_target.sh
```

- Failure:

```text
no I3C target devices found; see /tmp/qemu_ast2600_i3c_target_verify.log
```

- Fixed `linux/drivers/i3c/master.c` around the controller `bus_init` return handling.
- Fixed `linux/drivers/i3c/master/dw-i3c-master.c` around the CCC debug print and ENTDAA rejection path.
- Fixed `linux/drivers/i3c/device.c` around the private transfer debug print.

Before:

```c
ret = master->ops->bus_init(master);
if (ret)
pr_info("i3c: controller bus_init returned %d\n", ret);
        goto err_detach_devs;
```

After:

```c
ret = master->ops->bus_init(master);
pr_info("i3c: controller bus_init returned %d\n", ret);
if (ret)
        goto err_detach_devs;
```

## Results or Observations
- The old indentation made `goto err_detach_devs` unconditional.
- Because `ret` was zero on the successful controller `bus_init` path, the outer caller still saw success and printed `i3c_master_register OK`, but DAA and target registration were skipped.
- After fixing that, verification reached RSTDAA but failed with `ast2600-i3c-master ... failed with error -22`.
- A second debug-print control-flow bug was found in `dw_i3c_master_send_ccc_cmd()`: `return -EINVAL` was unconditional, so every CCC failed before hitting the QEMU model. The fix keeps the debug print unconditional and only rejects `I3C_CCC_ENTDAA`, preserving the DW driver's existing contract that ENTDAA is handled by `do_daa`.
- After fixing CCC handling, enumeration succeeded and `/sys/bus/i3c/devices` showed `0-123456789abc`, but the synthetic target test failed with `unexpected reset value 0x00`.
- A third debug-print control-flow bug was found in `i3c_device_do_xfers()`: `return 0` was unconditional, so private SDR xfers never reached the master driver and the caller's read buffer stayed unchanged.
- Rebuilt Linux zImage and reran the end-to-end verification script after fixing all three control-flow bugs.
- Final verification passed:

```text
0-123456789abc
```

- Final kernel log contains:

```text
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
```

## Follow-up Cleanup
- Removed the temporary high-volume I3C trace prints after the route was verified.
- The source-level cleanup leaves the functional AST2600 I3C route changes in place and avoids carrying debug-only kernel log noise forward.
- Rebuilt Linux after cleanup:

```sh
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage aspeed/aspeed-ast2600-evb.dtb
```

- Reran the end-to-end verification script after cleanup:

```sh
scripts/verify_ast2600_i3c_target.sh
```

- Cleanup verification passed:

```text
0-123456789abc
```

- Cleanup log still contains the SDR success line and no longer contains the temporary `ast2600-i3c:`, `dw-i3c:`, or generic `i3c:` trace prefixes.
