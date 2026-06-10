/*
 * ASPEED AST2600 I3C Controller
 *
 * This is a minimal DesignWare-compatible MMIO model for firmware and
 * Linux bring-up under the ast2600-evb machine. It models one synthetic
 * I3C target for DAA, basic CCC reads, and private SDR register transfers.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ASPEED_I3C_H
#define ASPEED_I3C_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_ASPEED_I3C "aspeed.i3c"
#define TYPE_ASPEED_2600_I3C TYPE_ASPEED_I3C "-ast2600"
OBJECT_DECLARE_TYPE(AspeedI3CState, AspeedI3CClass, ASPEED_I3C)

#define ASPEED_I3C_REG_SIZE 0x1000
#define ASPEED_I3C_NUM_REGS (ASPEED_I3C_REG_SIZE / sizeof(uint32_t))
#define ASPEED_I3C_RESP_QUEUE_SIZE 16
#define ASPEED_I3C_RX_FIFO_SIZE 64
#define ASPEED_I3C_TX_FIFO_SIZE 64
#define ASPEED_I3C_TARGET_REG_SIZE 256

struct AspeedI3CState {
    SysBusDevice parent;

    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t regs[ASPEED_I3C_NUM_REGS];
    uint32_t resp_queue[ASPEED_I3C_RESP_QUEUE_SIZE];
    uint8_t resp_rptr;
    uint8_t resp_wptr;
    uint8_t resp_count;
    bool have_cmd_hi;
    uint32_t pending_cmd_hi;
    uint8_t rx_fifo[ASPEED_I3C_RX_FIFO_SIZE];
    uint8_t rx_pos;
    uint8_t rx_len;
    uint8_t tx_fifo[ASPEED_I3C_TX_FIFO_SIZE];
    uint8_t tx_len;
    uint8_t target_regs[ASPEED_I3C_TARGET_REG_SIZE];
    uint8_t target_reg_ptr;
    bool target_present;
    bool target_assigned;
    uint8_t target_dyn_addr;
};

struct AspeedI3CClass {
    SysBusDeviceClass parent_class;
};

#endif /* ASPEED_I3C_H */
