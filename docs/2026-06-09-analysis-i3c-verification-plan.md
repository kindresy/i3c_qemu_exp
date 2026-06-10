# I3C Linux 设备驱动验证计划

## 目标路线

只保留一条路线：

```
qemu-system-arm -M ast2600-evb
        +
Linux ARM zImage
        +
aspeed-ast2600-evb.dtb
        +
ASPEED AST2600 I3C master driver
```

不再保留或维护其他 QEMU 启动路线。

## 当前入口

```bash
./boot_ast2600_i3c.sh
```

脚本使用：

```bash
qemu-system-arm \
  -M ast2600-evb \
  -kernel linux/arch/arm/boot/zImage \
  -dtb linux/arch/arm/boot/dts/aspeed/aspeed-ast2600-evb.dtb \
  -initrd ast2600_initramfs.cpio.gz \
  -append "console=ttyS4,115200n8 root=/dev/ram rw loglevel=8" \
  -nographic \
  -no-reboot
```

## 内核配置要求

当前 AST2600 路线需要这些配置内建：

```text
CONFIG_I3C=y
CONFIG_DW_I3C_MASTER=y
CONFIG_AST2600_I3C_MASTER=y
CONFIG_BLK_DEV_INITRD=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
```

## 设备树要求

AST2600 I3C 节点必须匹配当前内核绑定：

```dts
i3c0: i3c@1e7a2000 {
        compatible = "aspeed,ast2600-i3c";
        reg = <0x1e7a2000 0x1000>;
        clocks = <&syscon ASPEED_CLK_APB2>;
        resets = <&syscon ASPEED_RESET_I3C0>;
        aspeed,global-regs = <&syscon 0>;
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_i3c1_default>;
        interrupts = <GIC_SPI 102 IRQ_TYPE_LEVEL_HIGH>;
        #address-cells = <3>;
        #size-cells = <0>;
        status = "okay";
};
```

## Phase 1: 先验证 master probe

启动后检查：

```sh
dmesg | grep -i i3c
ls /sys/bus/i3c/
ls /sys/bus/i3c/devices/
ls /sys/bus/i3c/drivers/
```

通过标准：
- 内核能启动到 initramfs shell。
- `aspeed,ast2600-i3c` 对应 platform device 被创建。
- `ast2600-i3c-master` probe 日志没有明显 probe error。

当前本机 QEMU 6.2 可以启动 AST2600 EVB 并创建设备树中的 I3C platform device，但 `ast2600-i3c-master 1e7a2000.i3c` probe 结果是 `-110` 超时。下一步应补 QEMU AST2600 I3C 设备模型或确认寄存器模型，而不是回退到其他启动路线。

## Phase 2: target/DAA 验证

只有在 master probe 跑通后再推进：

1. 确认 QEMU 是否已有 AST2600 I3C target/总线模型。
2. 如果没有，给 QEMU `ast2600-evb` 增加最小 I3C target 模型。
3. 目标是让 Linux I3C core 执行 DAA 后在 `/sys/bus/i3c/devices/` 出现 target。

## Phase 3: client driver 验证

DAA 跑通后再添加最小 I3C client driver：

- probe 时打印 PID/BCR/DCR/Dynamic Address。
- 做一次 private SDR write。
- 做一次 private SDR read。

## Phase 4: IBI 和 Hot-Join

private SDR 读写稳定后再验证：

- SIR without payload
- SIR with MDB
- SIR with payload
- Hot-Join 触发重新枚举

## 测试矩阵

| Case | 目标 | 通过标准 |
| --- | --- | --- |
| boot_ast2600 | AST2600 QEMU 启动 | 进入 initramfs shell |
| i3c_master_probe | controller probe | dmesg 无 probe error |
| entdaa_one_target | DAA | `/sys/bus/i3c/devices` 有 target |
| driver_probe | client driver bind | probe 打印 PID/BCR/DCR |
| sdr_write_1b | private write | target 收到 offset |
| sdr_read_1b | private read | 读回正确 byte |
| sir_payload | IBI | handler 收到 payload |
| hotjoin | runtime attach | 新设备 DAA + probe |
