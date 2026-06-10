# I3C 驱动模块知识图谱与学习指引

- Timestamp: 2026-06-10
- Objective: 全面分析 Linux I3C 驱动子系统，生成知识图谱与学习路线
- Tool: Claude Code (understand-anything style analysis)

---

## 一、I3C 协议核心概念

### 1.1 I3C 是什么

I3C (Improved Inter-Integrated Circuit) 是 MIPI 联盟制定的协议，融合了 I2C 和 SPI 的优势：
- **向后兼容 I2C**：I3C 总线上可以挂 I2C 设备
- **更高速度**：I3C SCL 可达 12.5 MHz（I2C 最快 1 MHz）
- **动态地址**：运行时通过 DAA 分配，无需硬件拨码
- **带内中断 (IBI)**：设备可主动发起中断，不需要额外 IRQ 引脚
- **高速模式 (HDR)**：双倍数据率 (DDR)、三元符号等模式

### 1.2 核心术语表

| 术语 | 全称 | 含义 |
|------|------|------|
| CCC | Common Command Code | I3C 总线命令，类似 I2C 的 SMBus 呂令 |
| DAA | Dynamic Address Assignment | 动态地址分配流程 |
| PID | Provisioned ID | 48-bit 设备唯一标识（厂商ID + Part ID + 随机数） |
| BCR | Bus Characteristic Register | 描述设备总线能力 |
| DCR | Device Characteristic Register | 描述设备类型特征 |
| LVR | Legacy Virtual Register | I2C 设备在 I3C 总线上的虚拟寄存器 |
| IBI | In-Band Interrupt | 设备通过 SDA 发送中断 |
| SDR | Single Data Rate | 标准 8-bit 传输模式 |
| HDR | High Data Rate | DDR/TSP/TSL 高速模式 |
| Hot-Join | - | 新设备运行时动态加入总线 |

---

## 二、子系统架构总览

```
                    ┌──────────────────────────────────┐
                    │    Linux Device Model (sysfs)     │
                    │    /sys/bus/i3c/ /sys/bus/i2c/   │
                    └──────────────┬───────────────────┘
                                   │
                    ┌──────────────▼───────────────────┐
                    │       I3C Core Framework          │
                    │  device.c: 设备驱动注册/匹配       │
                    │  master.c: 总线管理/CCC/DAA       │
                    │  internals.h: 内部接口             │
                    └──────┬──────────────┬─────────────┘
                           │              │
              ┌────────────▼──┐     ┌────▼────────────┐
              │  I3C Device    │     │  I3C Master     │
              │  Driver API    │     │  Controller API  │
              │ (device.h)     │     │ (master.h)       │
              └───────────────┘     └────┬──────────────┘
                                         │
                    ┌────────────────────▼────────────────┐
                    │     Master Controller Drivers        │
                    │  dw-i3c-master (DesignWare)          │
                    │  i3c-master-cdns (Cadence)           │
                    │  svc-i3c-master (Silvaco)            │
                    │  mipi-i3c-hci (MIPI HCI)             │
                    │  ast2600-i3c-master (ASPEED/DW)      │
                    │  adi-i3c-master (Analog Devices)     │
                    │  renesas-i3c (Renesas)               │
                    └─────────────────────────────────────┘
```

---

## 三、文件组织与依赖关系

### 3.1 目录结构

```
drivers/i3c/
├── Kconfig                    # I3C 核心配置
├── Makefile                   # i3c-y := device.o master.o
├── device.c                   # 设备驱动框架
├── master.c                   # 主控制器核心 (3354行, 最关键)
├── internals.h                # 内部接口（非公开API）
├── i3c-synthetic-target-test.c # 测试驱动
└── master/                    # 硬件实现目录
    ├── Kconfig
    ├── Makefile
    ├── adi-i3c-master.c
    ├── ast2600-i3c-master.c   # ASPEED AST2600 (基于DW)
    ├── dw-i3c-master.c        # Synopsys DesignWare
    ├── dw-i3c-master.h        # DW 私有头文件
    ├── i3c-master-cdns.c      # Cadence
    ├── svc-i3c-master.c       # Silvaco 双角色
    ├── renesas-i3c.c          # Renesas
    └── mipi-i3c-hci/          # MIPI HCI 标准
        ├── cmd.h, cmd_v1.c, cmd_v2.c
        ├── core.c, dma.c, pio.c
        ├── dat.h, dat_v1.c
        ├── dct.h, dct_v1.c
        └── ...
```

### 3.2 头文件依赖图

```
include/linux/i3c/ccc.h           ← CCC 命令定义（所有文件依赖）
       │
       ▼
include/linux/i3c/master.h        ← 主控制器 API (ops 结构体)
       │
       ├── include/linux/i3c/device.h   ← 设备驱动 API
       │
       ▼
drivers/i3c/internals.h          ← 核心内部接口（非导出）
       │
       ├── drivers/i3c/device.c
       └── drivers/i3c/master.c

include/dt-bindings/i3c/i3c.h     ← DT 常量（LVR编码等）
```

---

## 四、核心数据结构关系图谱

### 4.1 结构体关系

```
struct i3c_master_controller        ──── 总线之主
  │
  ├── struct i3c_bus                 ──── 总线对象
  │     ├── struct i3c_dev_desc *cur_master
  │     ├── addrslots[]              ──── 地址位图
  │     ├── enum i3c_bus_mode        ──── PURE/MIXED-FAST/MIXED-LIMITED/MIXED-SLOW
  │     ├── scl_rate.i3c/i2c         ──── 时钟频率
  │     ├── devs.i3c/i2c             ──── 设备链表
  │     └── rw_semaphore lock        ──── 读写信号量
  │
  ├── struct i3c_dev_desc *this      ──── 自己作为I3C设备
  ├── struct i2c_adapter i2c         ──── I2C适配器（向后兼容）
  ├── ops *ops                       ──── 硬件操作回调
  ├── boardinfo.i3c/i2c              ──── 板级信息
  └── workqueue_struct *wq           ──── 工作队列

struct i3c_dev_desc                  ──── I3C设备描述符（内核内部）
  │
  ├── struct i3c_i2c_dev_desc common ──── 公共部分
  │     ├── struct i3c_dev_boardinfo *boardinfo (可选)
  │     ├── u8 addr                   ──── 动态/静态地址
  │     └── struct list_head node
  │
  ├── struct i3c_device_info info    ──── 设备能力
  │     ├── u64 pid                   ──── 48-bit 唯一标识
  │     ├── u8 bcr/dcr/static_addr/dyn_addr
  │     ├── u8 hdr_cap                ──── HDR 模式能力
  │     ├── max_read_ds/max_write_ds  ──── 最大数据速度
  │     └── max_ibi_len/max_read_turnaround
  │
  ├── struct i3c_device *dev         ──── 对外暴露的设备对象
  ├── mutex ibi_lock
  └── struct i3c_device_ibi_info *ibi ──── IBI 配置

struct i3c_device                    ──── 设备驱动可见的设备
  ├── struct device dev              ──── Linux设备模型
  ├── struct i3c_dev_desc *desc      ──── 内部描述符
  └── struct i3c_bus *bus            ──── 父总线
```

### 4.2 I2C 设备在 I3C 总线上

```
struct i3c_i2c_dev_desc              ──── I2C 设备描述符
  ├── struct i3c_dev_boardinfo *boardinfo
  ├── u8 addr                         ──── I2C 7-bit地址
  ├── u8 lvr                          ──── Legacy Virtual Register
  │     └── 编码: I2C_FM | I2C_FM_PLUS | I2C_FILTER | I2C_NO_FILTER
  └── struct list_head node
```

---

## 五、核心流程详解

### 5.1 主控制器注册与初始化

```
i3c_master_register(master)
  │
  ├── i3c_master_check_ops()           # 验证 ops 回调完整性
  ├── device_initialize()              # 初始化 device 结构
  ├── i3c_bus_init()                   # 初始化总线结构
  ├── of_populate_i3c_bus()            # 解析设备树
  │     ├── I3C设备: reg = <static_addr pid-high pid-low>
  │     └── I2C设备: reg = <i2c-addr 0 lvr>
  │
  ├── i3c_bus_set_mode()               # 设置总线模式
  │     └── 根据LVR判断: PURE/MIXED-FAST/MIXED-LIMITED/MIXED-SLOW
  │
  ├── i3c_master_bus_init()            # ★ 总线初始化完整流程
  │     ├── attach_i2c_boardinfo()     # 挂载 I2C 设备
  │     ├── ops->bus_init()            # 硬件初始化
  │     ├── RSTDAA                     # 重置所有动态地址
  │     ├── DISEC                      # 禁用从设备事件
  │     ├── SETDASA                    # 给有静态地址的I3C设备分配动态地址
  │     └── ENTDAA / do_daa()         # ★ 动态地址分配
  │
  ├── i3c_master_i2c_adapter_init()    # 注册 I2C 适配器
  ├── device_add()                     # 加入设备模型
  └── register_new_i3c_devs()          # 创建暴露给驱动的 i3c_device
```

### 5.2 DAA (动态地址分配) 流程

```
1. 主控制器发送 ENTDAA 广播命令
2. 从设备在 DAA 窗口响应:
   - 发送 48-bit PID
   - 发送 BCR (总线特征寄存器)
   - 发送 DCR (设备特征寄存器)
3. 主控制器分配 7-bit 动态地址
4. 对每个发现的设备:
   a. i3c_master_add_i3c_dev_locked()
   b. 查询设备信息: GETPID, GETBCR, GETDCR, GETMXDS, GETHDRCAP
   c. 尝试分配预期地址 (boardinfo 的 init_dyn_addr)
   d. 如设备之前已知, 恢复 IBI 配置
5. 完成后发送 DEFSLVS 通知辅助主控制器
```

### 5.3 总线锁机制

I3C 核心使用 **读写信号量** 保护总线:

| 操作 | 锁类型 | 场景 |
|------|--------|------|
| `i3c_bus_maintenance_lock()` | 写锁 | DAA、地址变更、Hot-Join、DEFSLVS |
| `i3c_bus_normaluse_lock()` | 读锁 | 常规 I3C/I2C 传输、CCC 读取 |

**关键设计**: 多个 `normaluse` 操作可并发（多个读锁），但 `maintenance` 操作独占总线。

### 5.4 CCC 命令体系

```
                    CCC 命令
                   /        \\
          广播命令            单播命令
         /    |    \\         /     \\
    ENTDAA RSTDAA DEFSLVS  SETDASA SETNEWDA
    ENEC  DISEC  ENTTM     GETPID  GETBCR
                            GETDCR  GETMXDS
                            GETHDRCAP
```

CCC 命令结构:
```c
struct i3c_ccc_cmd {
    u8 rnw;                     // 读/写方向
    u8 id;                      // CCC 命令 ID
    unsigned int ndests;        // 目标设备数
    struct i3c_ccc_cmd_dest *dests;  // 目标+载荷
    enum i3c_error_code err;   // 错误码
};
```

---

## 六、主控制器驱动实现模式

### 6.1 ops 回调结构体

每个主控制器驱动必须实现:

```c
struct i3c_master_controller_ops {
    // 必须实现
    int (*bus_init)(...);
    int (*do_daa)(...);
    int (*send_ccc_cmd)(...);
    int (*i3c_xfers)(...);     // SDR/HDR 传输
    int (*i2c_xfers)(...);     // I2C 传输

    // 可选
    void (*bus_cleanup)(...);
    int (*attach_i3c_dev)(...);
    int (*reattach_i3c_dev)(...);
    // ... IBI, Hot-Join 等
};
```

### 6.2 七种主控制器驱动对比

| 驱动 | IP 来源 | 特点 | 适用平台 |
|------|---------|------|---------|
| **DesignWare** | Synopsys | 最通用, 被多个SoC采用 | 通用IP |
| **Cadence** | Cadence | 独立实现 | Xilinx等 |
| **Silvaco** | Silvaco | 支持双角色(主/从) | 通用 |
| **MIPI HCI** | MIPI 标准 | 标准化接口, 支v1/v2 | PCI设备 |
| **ASPEED AST2600** | ASPEED | 封装DW IP + 全局寄存器 | BMC SoC |
| **Analog Devices** | AD | 独立实现 | AD平台 |
| **Renesas** | Renesas | RZ系列专用 | Renesas RZ |

### 6.3 ASPEED AST2600 特点

AST2600 是特殊案例——在 DesignWare IP 之上封装:
- 包含全局控制寄存器 (SDA pull-up, 模式切换)
- 多实例支持 (global_idx 区分)
- 寄存器操作通过 regmap

---

## 七、设备驱动编写指南

### 7.1 最小设备驱动模板

```c
#include <linux/i3c/device.h>
#include <linux/module.h>

// 1. 定义设备ID匹配表
static const struct i3c_device_id my_ids[] = {
    // 匹配方式: DCR/MANUF/PART/EXTRA_INFO
    I3C_DEVICE_EXTRA_INFO(manuf_id, part_id, extra_info, driver_data),
    { },  // 终止符
};
MODULE_DEVICE_TABLE(i3c, my_ids);

// 2. probe 函数
static int my_probe(struct i3c_device *i3cdev)
{
    struct device *dev = i3cdev_to_dev(i3cdev);
    // ... 初始化设备, 注册中断等
    return 0;
}

// 3. 定义驱动结构
static struct i3c_driver my_driver = {
    .driver = { .name = "my-i3c-driver" },
    .probe = my_probe,
    .id_table = my_ids,
};
module_i3c_driver(my_driver);
```

### 7.2 传输操作

```c
// 读寄存器 (SDR 模式)
struct i3c_xfer read_xfers[] = {
    { .rnw = false, .len = 1, .data.out = &reg },  // 写入寄存器地址
    { .rnw = true,  .len = 1, .data.in = val },     // 读取数据
};
i3c_device_do_xfers(dev, read_xfers, 2, I3C_SDR);

// 写寄存器
u8 buf[] = { reg, val };
struct i3c_xfer write_xfer = {
    .rnw = false, .len = sizeof(buf), .data.out = buf,
};
i3c_device_do_xfers(dev, &write_xfer, 1, I3C_SDR);
```

### 7.3 IBI (带内中断)

```c
struct i3c_ibi_setup setup = {
    .max_payload_len = 2,
    .num_slots = 4,
    .handler = my_ibi_handler,
};

i3c_device_request_ibi(dev, &setup);
i3c_device_enable_ibi(dev);
// ... 设备发送中断时 handler 被调用
i3c_device_disable_ibi(dev);
i3c_device_free_ibi(dev);
```

---

## 八、设备树绑定

### 8.1 主控制器节点

```dts
i3c@d040000 {
    compatible = "cdns,i3c-master";
    reg = <0x0d040000 0x1000>;
    #address-cells = <3>;     // I3C 设备需要 3 个地址单元
    #size-cells = <0>;
    i3c-scl-hz = <12500000>;  // I3C 时钟频率
    i2c-scl-hz = <100000>;    // I2C 时钟频率
};
```

### 8.2 I3C 子设备节点

```dts
// 有静态地址的 I3C 设备
thermal_sensor: sensor@68,39200144004 {
    reg = <0x68 0x392 0x144004>;   // static_addr, PID[47:16], PID[15:0]
    assigned-address = <0xa>;       // 请求的动态地址
};

// 无静态地址的 I3C 设备 (纯DAA发现)
pressure_sensor: sensor@0,39200124004 {
    reg = <0x0 0x392 0x124000>;
    assigned-address = <0xc>;
};
```

### 8.3 I2C 子设备节点

```dts
eeprom@57 {
    compatible = "atmel,24c01";
    reg = <0x57 0x0 (I2C_FM | I2C_FILTER)>;
    //              addr  0    LVR编码
};
```

LVR 编码常量 (`include/dt-bindings/i3c/i3c.h`):
```c
#define I2C_FM      (1 << 4)   // Fast Mode 400kHz
#define I2C_FM_PLUS (0 << 4)   // Fast Mode Plus 1MHz
#define I2C_FILTER  (0 << 5)   // 有 50ns spike filter
#define I2C_NO_FILTER_HIGH_FREQUENCY    (1 << 5)
#define I2C_NO_FILTER_LOW_FREQUENCY     (2 << 5)
```

---

## 九、调试接口 (Sysfs)

I3C 设备在 sysfs 暴露以下属性:

| 属性 | 权限 | 描述 |
|------|------|------|
| `mode` | R | 总线模式 |
| `current_master` | R | 当前主控制器 |
| `i3c_scl_frequency` | R | I3C 时频 |
| `i2c_scl_frequency` | R | I2C 时频 |
| `dynamic_address` | R | 动态地址 |
| `bcr`/`dcr`/`pid`/`hdrcap` | R | 设备能力 |
| `hotjoin` | RW | Hot-Join 开关 |
| `do_daa` | W | 触发 DAA |

---

## 十、学习路线推荐

### Phase 1: 协议理解 (1-2天)

1. 阅读 MIPI I3C 规范核心章节:
   - DAA 流程 (Section 5)
   - CCC 命令体系 (Section 6)
   - HDR 模式 (Section 7)
   - IBI 机制 (Section 8)
2. 理解 I3C vs I2C 的差异与兼容方式

### Phase 2: 核心代码阅读 (3-5天)

**阅读顺序 (从接口到实现):**

1. **头文件先行** (理解 API 契约):
   - `include/linux/i3c/ccc.h` → CCC 命令定义
   - `include/linux/i3c/device.h` → 设备驱动 API
   - `include/linux/i3c/master.h` → 主控制器 API
   - `include/dt-bindings/i3c/i3c.h` → DT 常量

2. **核心实现**:
   - `drivers/i3c/internals.h` → 内部接口
   - `drivers/i3c/device.c` → 设备注册与匹配
   - `drivers/i3c/master.c` → **最关键文件**, 3354行, 逐函数阅读:
     - 从 `i3c_master_register()` 入口
     - 追踪 DAA 流程
     - 理解总线锁机制
     - 分析 CCC 命令发送

3. **一个硬件驱动实例**:
   - `drivers/i3c/master/dw-i3c-master.c` → DesignWare (最通用)
   - 或 `drivers/i3c/master/ast2600-i3c-master.c` → 项目相关

### Phase 3: 设备驱动编写 (2-3天)

1. 参考 `i3c-synthetic-target-test.c` 作为模板
2. 学习设备匹配 (`i3c_device_id`) 机制
3. 理解 SDR 传输 (`i3c_device_do_xfers`)
4. 实践 IBI 注册流程

### Phase 4: 硬件驱动适配 (5-7天)

1. 理解 `i3c_master_controller_ops` 回调集
2. 阅读 DesignWare 驱动的寄存器操作
3. 学习设备树匹配与 probe 流程
4. 理解 I2C 适配器集成

### Phase 5: QEMU 模拟扩展 (项目特定)

1. QEMU I3C 模拟的 MMIO 实现
2. 秬有 SDR 传输的 QEMU 端支持
3. 合成目标设备的交互验证

---

## 十一、关键函数速查表

### master.c 核心函数

| 函数 | 行号参考 | 功能 |
|------|----------|------|
| `i3c_master_register()` | 入口 | 注册主控制器 |
| `i3c_master_bus_init()` | 核心 | 总线初始化全流程 |
| `i3c_master_do_daa()` | DAA | 执行动态地址分配 |
| `i3c_master_send_ccc_cmd_locked()` | CCC | 发送 CCC 命令 |
| `i3c_master_defslvs_locked()` | 同步 | 通知辅助主设备列表 |
| `i3c_bus_maintenance_lock()` | 锁 | 总线维护写锁 |
| `i3c_bus_normaluse_lock()` | 锆 | 总线常规读锁 |
| `i3c_master_add_i3c_dev_locked()` | 设备 | 添加I3C设备 |
| `i3c_master_reattach_i3c_dev_locked()` | 设备 | 重挂载设备 |

### device.c 核心函数

| 函数 | 功能 |
|------|------|
| `i3c_driver_register()` | 注册I3C设备驱动 |
| `i3c_device_match()` | 设备匹配 |
| `i3c_device_probe()` | 设备探测 |

### 设备 API (device.h)

| 函数 | 功能 |
|------|------|
| `i3c_device_do_xfers()` | 执行传输 |
| `i3c_device_request_ibi()` | 请求IBI |
| `i3c_device_enable_ibi()` | 启用IBI |
| `i3c_device_disable_ibi()` | 禁用IBI |
| `i3cdev_to_dev()` | 转换为device |

---

## 十二、观测记录

- I3C 子系统是一个**完整但较小**的 Linux 内核子系统
- `master.c` 是绝对核心, 包含总线管理、DAA、CCC、设备管理全部逻辑
- 总线锁的读写信号量设计值得学习, 是并发控制的经典模式
- ASPEED AST2600 是本项目关注的硬件平台
- `i3c-synthetic-target-test.c` 是项目已有的测试驱动, 展示了设备驱动编写模式

---

Next steps: 按 Phase 1-5 学习路线逐步深入, 优先完成 Phase 2 核心代码阅读
