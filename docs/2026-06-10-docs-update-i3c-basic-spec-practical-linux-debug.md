# 2026-06-10 Docs: I3C Basic Spec Practical Linux Debug Update

## Timestamp
2026-06-10 17:37 CST

## Category
Documentation

## Objective
Preserve the existing Obsidian I3C Basic Spec group-sharing article, then append a practical AST2600 QEMU + Linux I3C debugging section based on the current qemu_exp workflow.

## Target Document
`/home/luyuan/wiki/obsidian_kb/LLM_Wiki/raw/articles/i3c/I3C_Basic_Spec_组内分享.md`

## Backup
`/home/luyuan/wiki/obsidian_kb/LLM_Wiki/raw/articles/i3c/I3C_Basic_Spec_组内分享.md.bak-20260610-173719`

## Changes
- Kept the original article content intact.
- Appended `## 18. 实战：AST2600 QEMU + Linux I3C 调试闭环`.
- Added the current verification log path:
  `/tmp/qemu_ast2600_i3c_target_verify.log`
- Added the workflow trace branch:
  `debug/i3c-workflow-trace`
- Added Linux AST2600/DesignWare I3C call flow.
- Added QEMU AST2600 I3C model excerpts for DAA, CCC, and private SDR.
- Added the synthetic Linux I3C target test driver excerpt.
- Added debugging process notes and trace-print pitfalls.

## Verification
Confirmed the updated article starts with the exact backup content:

```text
backup_exists= True
original_preserved_prefix= True
backup_bytes= 26063
article_bytes= 40841
appended_bytes= 14778
```

Confirmed the appended section contains the expected anchors:

```text
## 18. 实战：AST2600 QEMU + Linux I3C 调试闭环
/tmp/qemu_ast2600_i3c_target_verify.log
debug/i3c-workflow-trace
aspeed_i3c_prepare_private_read
synth_i3c_probe
private SDR read/write OK
I3C_SYNTH_PID
```

## Next Steps
- If this article is later split into multiple Obsidian notes, keep the backup file until the split is reviewed.
- The trace branch is for learning workflow; do not treat the trace prints as production Linux patches without cleanup.
