# 2026-06-10 Ops: I3C Workflow Trace Branch

## Timestamp
2026-06-10 CST

## Objective
Create an independent branch that carries the Linux I3C workflow trace prints for learning the AST2600 I3C driver flow.

## Branch
`debug/i3c-workflow-trace`

## Code Changes
- Added `patches/linux/0005-i3c-add-workflow-trace-prints.patch`.
- The patch adds trace prints across:
  - AST2600 I3C platform probe
  - DesignWare common probe
  - I3C master registration and bus initialization
  - RSTDAA and DISEC before DAA
  - ENTDAA and response handling
  - GETPID/GETBCR/GETDCR CCC reads
  - private SDR transfers from the synthetic target test driver

## Verification
Built the traced Linux kernel:

```sh
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage aspeed/aspeed-ast2600-evb.dtb
```

Ran:

```sh
scripts/verify_ast2600_i3c_target.sh
```

Result:

```text
0-123456789abc
```

The captured log includes the expected private SDR success line:

```text
i3c-synthetic-target-test 0-123456789abc: private SDR read/write OK reg=0x10 value=0x5a
```

## Notes
- The top-level project remote is `https://github.com/kindresy/i3c_qemu_exp.git`.
- The nested `linux/` tree remote is `https://github.com/torvalds/linux.git`, so this branch stores the learning trace as a project patch instead of relying on pushing to the upstream Linux remote.
