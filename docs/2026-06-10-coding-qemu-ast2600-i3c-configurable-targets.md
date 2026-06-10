# 2026-06-10 Coding: QEMU AST2600 I3C Configurable Synthetic Targets

## Timestamp
2026-06-10 CST

## Category
Coding

## Objective
Make the AST2600 QEMU I3C synthetic target model configurable from the verification workflow, so one QEMU model can test different DAA target counts without changing C code.

## Changes
- Added QOM properties to the ASPEED I3C model:
  - `synth-target-count`
  - `synth-pid0`, `synth-pid1`
  - `synth-bcr0`, `synth-bcr1`
  - `synth-dcr0`, `synth-dcr1`
  - `synth-reset-value0`, `synth-reset-value1`
- Changed DAA, direct CCC identity reads, and reset initialization to use the active configured target count.
- Updated `boot_ast2600_i3c.sh` to accept `QEMU_I3C_TARGET_COUNT`.
- Updated `scripts/verify_ast2600_i3c_target.sh` to accept `EXPECT_TARGETS` and enforce exact target count.

## Debug Note
The first command-line attempt used this form:

```sh
-global aspeed.i3c-ast2600.synth-target-count=1
```

QEMU parsed the first dot as the driver/property separator and reported:

```text
global aspeed.i3c-ast2600.synth-target-count has invalid class name
```

The working form is the key/value form, which preserves the dotted QOM type name:

```sh
-global driver=aspeed.i3c-ast2600,property=synth-target-count,value=1
```

## Commands
Built QEMU:

```sh
ninja -C qemu-6.2+dfsg/build-arm-softmmu qemu-system-arm
```

Ran single-target verification:

```sh
QEMU_I3C_TARGET_COUNT=1 EXPECT_TARGETS="0-123456789abc" LOG=/tmp/qemu_ast2600_i3c_target_count1_verify.log scripts/verify_ast2600_i3c_target.sh
```

Ran default two-target verification:

```sh
scripts/verify_ast2600_i3c_target.sh
```

## Verification Result
Single-target output:

```text
0-123456789abc
```

Default output:

```text
0-123456789abc
0-123456789abd
```

## Result
The AST2600 I3C model can now validate both the default two-target topology and a reduced one-target topology through environment variables in the existing boot and verification scripts.

## Next Steps
- Add a configured PID/BCR/DCR verification case once Linux test coverage needs identity-value assertions.
- Use the target-count knob as the base for a later hotjoin/late-target test.
