# 2026-06-10 Coding: QEMU AST2600 I3C IBI Validation

## Timestamp
2026-06-10 CST

## Category
Coding

## Objective
Add a minimal In-Band Interrupt validation path for the AST2600 QEMU I3C model and the Linux synthetic I3C target test driver.

## Changes
- Extended the Linux synthetic target test driver to:
  - request IBI slots with `i3c_device_request_ibi()`
  - enable IBI with `i3c_device_enable_ibi()`
  - log the received one-byte payload from the IBI handler
- Extended the QEMU AST2600 I3C model to:
  - recognize direct `ENEC` with `I3C_CCC_EVENT_SIR`
  - enqueue one DW-compatible IBI status entry
  - expose IBI count through `QUEUE_STATUS_LEVEL`
  - return IBI status and payload through `IBI_QUEUE_STATUS`
  - assert `INTR_IBI_THLD_STAT`
- Extended `scripts/verify_ast2600_i3c_target.sh` to require the IBI handler log line.
- Synced `patches/linux/0004-i3c-add-synthetic-target-test-source.patch`.

## RED
After adding the Linux driver handler and script assertion, verification failed before QEMU IBI support:

```text
I3C synthetic target IBI test did not pass; see /tmp/qemu_ast2600_i3c_ibi_red_verify.log
```

The same log showed Linux did request and enable IBI:

```text
dw-i3c: send_ccc ccc_id=0x80 ndests=1 rnw=0
i3c-synthetic-target-test 0-123456789abc: IBI enabled
```

## Commands
Built Linux:

```sh
make -C linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j2 zImage aspeed/aspeed-ast2600-evb.dtb
```

Built QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Ran default verification:

```sh
LOG=/tmp/qemu_ast2600_i3c_ibi_verify.log scripts/verify_ast2600_i3c_target.sh
```

Ran late hotjoin plus IBI verification:

```sh
VERIFY_I3C_LATE_HOTJOIN=1 QEMU_I3C_TARGET_COUNT=1 QEMU_I3C_LATE_TARGET_COUNT=1 EXPECT_TARGETS="0-123456789abc 0-123456789abd" LOG=/tmp/qemu_ast2600_i3c_late_hotjoin_ibi_verify.log scripts/verify_ast2600_i3c_target.sh
```

## Verification Result
Default output:

```text
0-123456789abc
0-123456789abd
```

Late hotjoin plus IBI output:

```text
0-123456789abc
0-123456789abd
```

Relevant IBI log lines:

```text
dw-i3c: IRQ status=0x14
i3c-synthetic-target-test 0-123456789abc: IBI enabled
i3c-synthetic-target-test 0-123456789abc: IBI received len=1 payload=0x7c
```

## Result
The validation flow now covers:

- DAA
- direct CCC identity reads
- private SDR read/write
- malformed private SDR rejection
- hotjoin sysfs control
- late target discovery through a second DAA pass
- minimal SIR IBI delivery and Linux handler execution

## Next Steps
- Convert the current source-of-record files into a clean QEMU/Linux patch series.
- Update the I3C Basic Spec teaching article with the full practical Linux debug workflow.
