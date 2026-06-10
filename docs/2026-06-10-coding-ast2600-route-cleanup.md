# 2026-06-10-coding-ast2600-route-cleanup

## Timestamp
2026-06-10 CST

## Objective
按用户要求将工程收敛到 ASPEED/AST2600 QEMU 机器路线，并删除其他路线相关内容。

## Commands Used / Code Changes

Commands:

```bash
qemu-system-arm -machine help
find . -maxdepth 3 -type f -not -path './linux/*' -printf '%p\n'
ls -lh linux/arch/arm/boot/zImage linux/arch/arm/boot/dts/aspeed/*.dtb
rg -n "<non-AST2600 route patterns>" -S . -g '!linux/**'
git -C linux status --short
rm -rf <non-AST2600 route artifacts>
make -C linux ARCH=arm aspeed/aspeed-ast2600-evb.dtb
timeout 15s ./boot_ast2600_i3c.sh
gzip -dc ast2600_initramfs.cpio.gz | cpio -it
```

Code changes:

- Added `boot_ast2600_i3c.sh` as the single QEMU boot entrypoint.
- Removed non-AST2600 route artifacts.
- Rewrote `docs/2026-06-09-analysis-i3c-verification-plan.md` to describe only the AST2600 route.
- Adjusted `linux/arch/arm/boot/dts/aspeed/aspeed-g6.dtsi` I3C node to match the current `aspeed,ast2600-i3c` binding.

## Results or Observations

- Local QEMU supports `ast2600-evb`.
- The project already has `linux/arch/arm/boot/zImage`, `linux/arch/arm/boot/dts/aspeed/aspeed-ast2600-evb.dtb`, and `ast2600_initramfs.cpio.gz`.
- The AST2600 I3C driver is built in through `CONFIG_AST2600_I3C_MASTER=y`.
- The cleaned AST2600 initramfs no longer carries external I3C modules.
- A short QEMU boot reaches `/init` and the shell.
- The AST2600 I3C platform device is created, but `ast2600-i3c-master 1e7a2000.i3c` currently fails probe with `-110`, consistent with missing or incomplete I3C MMIO behavior in the QEMU AST2600 model.

## Next Steps

- Add or validate the QEMU AST2600 I3C device model so the Linux master driver can complete probe.
