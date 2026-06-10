# 2026-06-10 Git Push Strategy

## Timestamp
2026-06-10

## Objective
Decide which files belong in the git repository and push to
`https://github.com/kindresy/i3c_qemu_exp.git`.

## File Classification

### Worth pushing (project IP)
| File | Reason |
|------|--------|
| `CLAUDE.md` | Project rules, team collaboration required |
| `boot_ast2600_i3c.sh` | Reproducible boot script, core entry point |
| `scripts/verify_ast2600_i3c_target.sh` | Automated verification script |
| `docs/` (all 6 files) | Work process records, institutional knowledge |
| `patches/` (Linux + QEMU) | Self-authored modifications extracted as patches |
| `linux/drivers/i3c/i3c-synthetic-target-test.c` | Self-authored test driver source |
| `qemu-6.2+dfsg/include/hw/misc/aspeed_i3c.h` | Self-authored I3C model header |
| `qemu-6.2+dfsg/hw/misc/aspeed_i3c.c` | Self-authored I3C model source |

### Not worth pushing (downstream / binary / regenerable)
| File | Reason |
|------|--------|
| `linux/` (4.9G full kernel tree) | Downstream source + build artifacts; use patches |
| `qemu-6.2+dfsg/` (510M full tree) | Downstream source + build artifacts; use patches |
| `ast2600_initramfs.cpio.gz` (660K) | Binary artifact, regenerable |
| `qemu_6.2+dfsg.orig.tar.xz` (22M) | Upstream source tarball, re-downloadable |
| `qemu_6.2+dfsg-2ubuntu6.30.debian.tar.xz` | Debian patch tarball, re-downloadable |
| `qemu_6.2+dfsg-2ubuntu6.30.dsc` | Debian metadata, re-downloadable |

## Repository Structure

```
i3c_qemu_exp/
├── CLAUDE.md
├── boot_ast2600_i3c.sh
├── scripts/
│   └── verify_ast2600_i3c_target.sh
├── docs/
│   └── ...
├── patches/
│   ├── linux/
│   │   ├── 0001-dts-aspeed-g6-add-i3c-node.patch
│   │   ├── 0002-i3c-add-synthetic-target-test-Kconfig.patch
│   │   └── 0003-i3c-add-synthetic-target-test-driver.patch
│   └── qemu/
│       ├── 0001-misc-add-aspeed-i3c-device-model.patch
│       └── 0002-arm-aspeed-wire-i3c-into-ast2600-soc.patch
├── qemu-mods/
│   ├── hw/misc/aspeed_i3c.c
│   └── include/hw/misc/aspeed_i3c.h
├── linux-mods/
│   └── drivers/i3c/i3c-synthetic-target-test.c
└── .gitignore
```

## Strategy
- Downstream repos (`linux/`, `qemu-6.2+dfsg/`) are .gitignored
- Self-authored source files are mirrored in `qemu-mods/` and `linux-mods/`
- Patch files in `patches/` allow applying changes to fresh downstream clones
- Binary artifacts and upstream tarballs are .gitignored

## Next Steps
- Execute the push to GitHub
