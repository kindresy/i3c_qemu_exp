/*
 * ASPEED AST2600 I3C Controller
 *
 * Minimal DesignWare-compatible controller model used to let the AST2600
 * Linux I3C master driver complete command transfers in QEMU. The model
 * includes one synthetic I3C target for DAA, basic CCC reads, and private
 * SDR register-transfer validation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/misc/aspeed_i3c.h"
#include "migration/vmstate.h"

#define I3C_COMMAND_QUEUE_PORT          0x0c
#define I3C_RESPONSE_QUEUE_PORT         0x10
#define I3C_RX_TX_DATA_PORT             0x14
#define I3C_RESET_CTRL                  0x34
#define I3C_INTR_STATUS                 0x3c
#define I3C_INTR_STATUS_EN              0x40
#define I3C_INTR_SIGNAL_EN              0x44
#define I3C_INTR_FORCE                  0x48
#define I3C_INTR_RESP_READY_STAT        BIT(4)
#define I3C_QUEUE_STATUS_LEVEL          0x4c
#define I3C_DATA_BUFFER_STATUS_LEVEL    0x50
#define I3C_DEVICE_ADDR_TABLE_POINTER   0x5c
#define I3C_VER_ID                      0xe0
#define I3C_VER_TYPE                    0xe4

#define I3C_COMMAND_PORT_TID(x)         (((x) >> 3) & 0xf)
#define I3C_COMMAND_PORT_CMD(x)         (((x) >> 7) & 0xff)
#define I3C_COMMAND_PORT_DEV_INDEX(x)   (((x) >> 16) & 0x1f)
#define I3C_COMMAND_PORT_ARG_LEN(x)     (((x) >> 16) & 0xffff)
#define I3C_COMMAND_PORT_CP             BIT(15)
#define I3C_COMMAND_PORT_READ_TRANSFER  BIT(28)
#define I3C_RESPONSE_PORT_TID(x)        (((x) & 0xf) << 24)
#define I3C_RESPONSE_PORT_ERR(x)        (((x) & 0xf) << 28)

#define I3C_CCC_ENTDAA                  0x07
#define I3C_CCC_GETPID                  0x8d
#define I3C_CCC_GETBCR                  0x8e
#define I3C_CCC_GETDCR                  0x8f
#define I3C_RESPONSE_ERROR_IBA_NACK     4
#define I3C_CMD_FIFO_DEPTH              16
#define I3C_DATA_FIFO_DEPTH             16
#define I3C_DAT_DEPTH                   8
#define I3C_DAT_START_ADDR              0x100

#define I3C_DAT_DYNAMIC_ADDR(x)         (((x) >> 16) & 0x7f)

#define I3C_SYNTH_PID                   0x123456789abcULL
#define I3C_SYNTH_BCR                   0x00
#define I3C_SYNTH_DCR                   0x42
#define I3C_SYNTH_TEST_REG              0x10
#define I3C_SYNTH_TEST_RESET_VALUE      0xa5

#define TO_REG(addr) ((addr) / sizeof(uint32_t))

static void aspeed_i3c_update_irq(AspeedI3CState *s)
{
    if (s->regs[TO_REG(I3C_INTR_STATUS)] &
        s->regs[TO_REG(I3C_INTR_STATUS_EN)] &
        s->regs[TO_REG(I3C_INTR_SIGNAL_EN)]) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static void aspeed_i3c_clear_rx_fifo(AspeedI3CState *s)
{
    memset(s->rx_fifo, 0, sizeof(s->rx_fifo));
    s->rx_pos = 0;
    s->rx_len = 0;
}

static void aspeed_i3c_load_rx_fifo(AspeedI3CState *s, const uint8_t *buf,
                                    uint8_t len)
{
    len = MIN(len, (uint8_t)sizeof(s->rx_fifo));
    memcpy(s->rx_fifo, buf, len);
    s->rx_pos = 0;
    s->rx_len = len;
}

static uint32_t aspeed_i3c_read_rx_fifo(AspeedI3CState *s)
{
    uint32_t value = 0;
    int i;

    for (i = 0; i < 4 && s->rx_pos < s->rx_len; i++, s->rx_pos++) {
        value |= s->rx_fifo[s->rx_pos] << (i * 8);
    }

    if (s->rx_pos >= s->rx_len) {
        aspeed_i3c_clear_rx_fifo(s);
    }

    return value;
}

static void aspeed_i3c_write_tx_fifo(AspeedI3CState *s, uint32_t value)
{
    int i;

    for (i = 0; i < 4 && s->tx_len < sizeof(s->tx_fifo); i++) {
        s->tx_fifo[s->tx_len++] = extract32(value, i * 8, 8);
    }
}

static void aspeed_i3c_clear_tx_fifo(AspeedI3CState *s)
{
    memset(s->tx_fifo, 0, sizeof(s->tx_fifo));
    s->tx_len = 0;
}

static void aspeed_i3c_prepare_private_read(AspeedI3CState *s, uint32_t len)
{
    uint8_t payload[ASPEED_I3C_RX_FIFO_SIZE];
    uint8_t n = MIN(len, (uint32_t)sizeof(payload));
    int i;

    for (i = 0; i < n; i++) {
        payload[i] = s->target_regs[(uint8_t)(s->target_reg_ptr + i)];
    }
    s->target_reg_ptr += n;
    aspeed_i3c_load_rx_fifo(s, payload, n);
}

static void aspeed_i3c_apply_private_write(AspeedI3CState *s, uint32_t len)
{
    uint8_t n = MIN(len, (uint32_t)s->tx_len);
    int i;

    if (!n) {
        aspeed_i3c_clear_tx_fifo(s);
        return;
    }

    s->target_reg_ptr = s->tx_fifo[0];
    for (i = 1; i < n; i++) {
        s->target_regs[s->target_reg_ptr++] = s->tx_fifo[i];
    }

    aspeed_i3c_clear_tx_fifo(s);
}

static void aspeed_i3c_prepare_ccc_read(AspeedI3CState *s, uint32_t cmd_hi,
                                        uint32_t cmd_lo, uint32_t *data_len,
                                        uint32_t *error)
{
    uint8_t payload[8];
    uint32_t len = I3C_COMMAND_PORT_ARG_LEN(cmd_hi);

    *data_len = 0;
    *error = 0;
    aspeed_i3c_clear_rx_fifo(s);

    if (!(cmd_lo & I3C_COMMAND_PORT_READ_TRANSFER)) {
        return;
    }

    switch (I3C_COMMAND_PORT_CMD(cmd_lo)) {
    case I3C_CCC_GETPID:
        payload[0] = extract64(I3C_SYNTH_PID, 40, 8);
        payload[1] = extract64(I3C_SYNTH_PID, 32, 8);
        payload[2] = extract64(I3C_SYNTH_PID, 24, 8);
        payload[3] = extract64(I3C_SYNTH_PID, 16, 8);
        payload[4] = extract64(I3C_SYNTH_PID, 8, 8);
        payload[5] = extract64(I3C_SYNTH_PID, 0, 8);
        *data_len = MIN(len, 6U);
        aspeed_i3c_load_rx_fifo(s, payload, *data_len);
        break;
    case I3C_CCC_GETBCR:
        payload[0] = I3C_SYNTH_BCR;
        *data_len = MIN(len, 1U);
        aspeed_i3c_load_rx_fifo(s, payload, *data_len);
        break;
    case I3C_CCC_GETDCR:
        payload[0] = I3C_SYNTH_DCR;
        *data_len = MIN(len, 1U);
        aspeed_i3c_load_rx_fifo(s, payload, *data_len);
        break;
    default:
        break;
    }
}

static void aspeed_i3c_push_response(AspeedI3CState *s, uint32_t cmd_lo)
{
    uint32_t tid = I3C_COMMAND_PORT_TID(cmd_lo);
    uint32_t data_len = 0;
    uint32_t error = 0;
    uint32_t dev_index = I3C_COMMAND_PORT_DEV_INDEX(cmd_lo);

    if (s->resp_count == ASPEED_I3C_RESP_QUEUE_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: response queue overflow\n",
                      __func__);
        return;
    }

    switch (I3C_COMMAND_PORT_CMD(cmd_lo)) {
    case I3C_CCC_ENTDAA:
        if (s->target_present && !s->target_assigned &&
            dev_index < I3C_DAT_DEPTH) {
            hwaddr dat_addr = I3C_DAT_START_ADDR + dev_index * 4;

            s->target_dyn_addr =
                I3C_DAT_DYNAMIC_ADDR(s->regs[TO_REG(dat_addr)]);
            s->target_assigned = true;
            data_len = I3C_DAT_DEPTH - dev_index - 1;
            error = I3C_RESPONSE_ERROR_IBA_NACK;
        } else {
            data_len = I3C_DAT_DEPTH - MIN(dev_index, I3C_DAT_DEPTH);
            error = I3C_RESPONSE_ERROR_IBA_NACK;
        }
        break;
    default:
        if (cmd_lo & I3C_COMMAND_PORT_CP) {
            aspeed_i3c_prepare_ccc_read(s, s->pending_cmd_hi, cmd_lo,
                                        &data_len, &error);
        } else if (cmd_lo & I3C_COMMAND_PORT_READ_TRANSFER) {
            data_len = I3C_COMMAND_PORT_ARG_LEN(s->pending_cmd_hi);
            data_len = MIN(data_len, (uint32_t)ASPEED_I3C_RX_FIFO_SIZE);
            aspeed_i3c_prepare_private_read(s, data_len);
        } else {
            data_len = 0;
            aspeed_i3c_apply_private_write(
                s, I3C_COMMAND_PORT_ARG_LEN(s->pending_cmd_hi));
        }
        break;
    }

    s->resp_queue[s->resp_wptr] = I3C_RESPONSE_PORT_ERR(error) |
                                  I3C_RESPONSE_PORT_TID(tid) |
                                  data_len;
    s->resp_wptr = (s->resp_wptr + 1) % ASPEED_I3C_RESP_QUEUE_SIZE;
    s->resp_count++;

    s->regs[TO_REG(I3C_INTR_STATUS)] |= I3C_INTR_RESP_READY_STAT;
    aspeed_i3c_update_irq(s);
}

static uint32_t aspeed_i3c_pop_response(AspeedI3CState *s)
{
    uint32_t value;

    if (!s->resp_count) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: response queue underflow\n",
                      __func__);
        return 0;
    }

    value = s->resp_queue[s->resp_rptr];
    s->resp_rptr = (s->resp_rptr + 1) % ASPEED_I3C_RESP_QUEUE_SIZE;
    s->resp_count--;

    if (!s->resp_count) {
        s->regs[TO_REG(I3C_INTR_STATUS)] &= ~I3C_INTR_RESP_READY_STAT;
        aspeed_i3c_update_irq(s);
    }

    return value;
}

static void aspeed_i3c_reset_fifos(AspeedI3CState *s)
{
    memset(s->resp_queue, 0, sizeof(s->resp_queue));
    s->resp_rptr = 0;
    s->resp_wptr = 0;
    s->resp_count = 0;
    s->have_cmd_hi = false;
    s->pending_cmd_hi = 0;
    aspeed_i3c_clear_rx_fifo(s);
    aspeed_i3c_clear_tx_fifo(s);
    s->regs[TO_REG(I3C_INTR_STATUS)] &= ~I3C_INTR_RESP_READY_STAT;
    aspeed_i3c_update_irq(s);
}

static uint64_t aspeed_i3c_read(void *opaque, hwaddr addr, unsigned int size)
{
    AspeedI3CState *s = opaque;

    if (addr >= ASPEED_I3C_REG_SIZE) {
        return 0;
    }

    switch (addr) {
    case I3C_RESPONSE_QUEUE_PORT:
        return aspeed_i3c_pop_response(s);
    case I3C_RX_TX_DATA_PORT:
        return aspeed_i3c_read_rx_fifo(s);
    case I3C_RESET_CTRL:
        return 0;
    case I3C_QUEUE_STATUS_LEVEL:
        return (s->resp_count << 8) | I3C_CMD_FIFO_DEPTH;
    case I3C_DATA_BUFFER_STATUS_LEVEL:
        return I3C_DATA_FIFO_DEPTH;
    case I3C_DEVICE_ADDR_TABLE_POINTER:
        return (I3C_DAT_DEPTH << 16) | I3C_DAT_START_ADDR;
    case I3C_VER_ID:
        return 0x3130302a;
    case I3C_VER_TYPE:
        return 0x6c633033;
    default:
        return s->regs[TO_REG(addr)];
    }
}

static void aspeed_i3c_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned int size)
{
    AspeedI3CState *s = opaque;
    uint32_t val32 = value;

    if (addr >= ASPEED_I3C_REG_SIZE) {
        return;
    }

    switch (addr) {
    case I3C_COMMAND_QUEUE_PORT:
        if (s->have_cmd_hi) {
            s->have_cmd_hi = false;
            aspeed_i3c_push_response(s, val32);
        } else {
            s->pending_cmd_hi = val32;
            s->have_cmd_hi = true;
        }
        break;
    case I3C_RX_TX_DATA_PORT:
        aspeed_i3c_write_tx_fifo(s, val32);
        break;
    case I3C_RESET_CTRL:
        aspeed_i3c_reset_fifos(s);
        break;
    case I3C_INTR_STATUS:
        s->regs[TO_REG(addr)] &= ~val32;
        aspeed_i3c_update_irq(s);
        break;
    case I3C_INTR_STATUS_EN:
    case I3C_INTR_SIGNAL_EN:
        s->regs[TO_REG(addr)] = val32;
        aspeed_i3c_update_irq(s);
        break;
    case I3C_INTR_FORCE:
        s->regs[TO_REG(I3C_INTR_STATUS)] |= val32;
        aspeed_i3c_update_irq(s);
        break;
    default:
        s->regs[TO_REG(addr)] = val32;
        break;
    }
}

static const MemoryRegionOps aspeed_i3c_ops = {
    .read = aspeed_i3c_read,
    .write = aspeed_i3c_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void aspeed_i3c_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AspeedI3CState *s = ASPEED_I3C(dev);

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &aspeed_i3c_ops, s,
                          TYPE_ASPEED_I3C, ASPEED_I3C_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void aspeed_i3c_reset(DeviceState *dev)
{
    AspeedI3CState *s = ASPEED_I3C(dev);

    memset(s->regs, 0, sizeof(s->regs));
    memset(s->target_regs, 0, sizeof(s->target_regs));
    s->target_regs[I3C_SYNTH_TEST_REG] = I3C_SYNTH_TEST_RESET_VALUE;
    s->target_reg_ptr = 0;
    s->target_present = true;
    s->target_assigned = false;
    s->target_dyn_addr = 0;
    aspeed_i3c_reset_fifos(s);
}

static const VMStateDescription aspeed_i3c_vmstate = {
    .name = TYPE_ASPEED_I3C,
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AspeedI3CState, ASPEED_I3C_NUM_REGS),
        VMSTATE_UINT32_ARRAY(resp_queue, AspeedI3CState,
                             ASPEED_I3C_RESP_QUEUE_SIZE),
        VMSTATE_UINT8(resp_rptr, AspeedI3CState),
        VMSTATE_UINT8(resp_wptr, AspeedI3CState),
        VMSTATE_UINT8(resp_count, AspeedI3CState),
        VMSTATE_BOOL(have_cmd_hi, AspeedI3CState),
        VMSTATE_UINT32(pending_cmd_hi, AspeedI3CState),
        VMSTATE_UINT8_ARRAY(rx_fifo, AspeedI3CState, ASPEED_I3C_RX_FIFO_SIZE),
        VMSTATE_UINT8(rx_pos, AspeedI3CState),
        VMSTATE_UINT8(rx_len, AspeedI3CState),
        VMSTATE_UINT8_ARRAY(tx_fifo, AspeedI3CState, ASPEED_I3C_TX_FIFO_SIZE),
        VMSTATE_UINT8(tx_len, AspeedI3CState),
        VMSTATE_UINT8_ARRAY(target_regs, AspeedI3CState,
                            ASPEED_I3C_TARGET_REG_SIZE),
        VMSTATE_UINT8(target_reg_ptr, AspeedI3CState),
        VMSTATE_BOOL(target_present, AspeedI3CState),
        VMSTATE_BOOL(target_assigned, AspeedI3CState),
        VMSTATE_UINT8(target_dyn_addr, AspeedI3CState),
        VMSTATE_END_OF_LIST(),
    },
};

static void aspeed_i3c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "ASPEED AST2600 minimal I3C Controller";
    dc->realize = aspeed_i3c_realize;
    dc->reset = aspeed_i3c_reset;
    dc->vmsd = &aspeed_i3c_vmstate;
}

static const TypeInfo aspeed_i3c_info = {
    .name = TYPE_ASPEED_I3C,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AspeedI3CState),
    .class_init = aspeed_i3c_class_init,
    .class_size = sizeof(AspeedI3CClass),
};

static const TypeInfo aspeed_2600_i3c_info = {
    .name = TYPE_ASPEED_2600_I3C,
    .parent = TYPE_ASPEED_I3C,
};

static void aspeed_i3c_register_type(void)
{
    type_register_static(&aspeed_i3c_info);
    type_register_static(&aspeed_2600_i3c_info);
}
type_init(aspeed_i3c_register_type);
