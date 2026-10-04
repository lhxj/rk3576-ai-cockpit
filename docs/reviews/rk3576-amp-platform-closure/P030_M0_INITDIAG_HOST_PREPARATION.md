> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 M0 initialization diagnostic: build and passive staging

Date: 2026-10-04
State: Historical v1 build/staging record; v1 SCRIPT component terminator is incompatible with the actual parser. Use source-fix-v2.

## 2026-10-04 correction

The prior CRC/size checks missed the actual source parser requirement that the second component-table word be zero. The pinned v1 file has 0xffffffff and is rejected before script dispatch. Actual source regression now proves rejection of v1 and acceptance of v2 (190 checks). Only the terminator and CRCs changed; CMD/FIT/firmware unchanged. Passive source-fix-v2 staging and independent readback pass. The latest full COM5 confirms default cold recovery and shows a distinct source-before-load rejection. See [execution record](P030_INITDIAG_B_EXECUTION.md), [new guide](P030_INITDIAG_SOURCE_FIX_V2_GUIDE.md), and [machine evidence](P030_INITDIAG_SOURCE_FIX_V2.json). Historical v1 checks below do not establish source compatibility or B execution.

## Diagnosis scope

The prior user-observed B run reached the M0 entry/cache output, then stopped without the waiting heartbeat or 15-second link timeout. Source review showed `remote_init` is synchronous and the 15-second deadline starts only after it returns. Missing later output does not identify whether initialization returned, RT ticks advanced, or a mapping/IRQ/clock/allocator issue caused the stall.

The review found a mailbox client pointer array indexed by remote ID 4 despite a one-entry local array. The actual P029 ELF optimized that local array away, so the correction is prudent but is not presented as the proven cause of the stall.

## Source changes and Host result

- `0011-rtthread-mbox-client-pointer.patch` replaces the local pointer array with a scalar pointer set immediately before each `HAL_MBOX_RegisterClient` call.
- `0012-m0-init-checkpoints.patch` logs `remote_init` entry/return and tick delta, first link probe entry/return, first 1000 ms delay entry/return and tick delta, then at most three waiting heartbeat lines.
- The existing 15-second link deadline still begins after `remote_init` returns. Shared-memory addresses, M0 load address, FIT payload entry, transport configuration, and B-to-Linux boot path are unchanged.
- A clean SCons build with ARM GNU Toolchain 13.2.rel1 completed with zero warning/error matches.

ELF: 754916 bytes, SHA256 `33651b1becd502f93f2b96dad46ea5cccf6698b17174f7afc60e3c6f25de7054`.
Raw BIN: 123816 bytes, SHA256 `e8b04bba4cbd88807cfda4366da6f0111ed6903075b2b3096ace000b6ef1579f`; load `0x47800000`, entry `0x141`.
Build log SHA256: `d4e046dda2a1f1fe318f9b7b8f5f8c4907fc38c9e1c023abb2e3c2c87af21719`.

## Signed FIT and B script

The new BIN was inserted into the P029 external-data FIT layout and signed with the existing local P029 development test key. The matching fixed vendor `fit_check_sign` returned `Signature check OK`; the control-key DTB requires the configuration signature. This is Host container evidence only, not target hardware verification.

- FIT: 129024 bytes (`0x1f800`), SHA256 `cc1c99d5532e5cc1cd63e690b5d205964ba75a7f68692eff3282f02ec308df30`.
- Payload: data position `0x1000`, size `0x1e3a8`, SHA256 matches the raw BIN, load `0x47800000`, entry `0x141`.
- FIT verifier SHA256: `3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8`.
- `stage-B.cmd`: 2990 bytes, SHA256 `593bf63f7e7ce785de22e9068a37ee75c7f75b3c614b9005ffdf6fd16d24e6f8`.
- Vendor-generated `stage-B.scr`: 3062 bytes, SHA256 `68580407d0b0961e27a8938950aeb42b967e9bd4b534cdd23c1625d4f1d32642`; exact FIT size gate is `0x1f800`.

The script contains one M0 loader call, no KO or C stage, and boots the paired P029 Linux image only after successful loader return. It was not designed to execute from Linux.

## Default Linux board baseline

After the user reported a full restore, an authenticated bounded read-only SSH snapshot confirmed:

- Board: EmbedFire LubanCat-3-v2; kernel `6.1.99-rk3576 #8`.
- Root `/dev/mmcblk0p3`; boot `/dev/mmcblk0p2`; command line includes `uboot-149b1c5` and no `amp_test_stage=`.
- Full 8 MiB `/dev/mmcblk0p1` SHA256 `f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3`, matching the P030 proper-FDT U-Boot image.
- The six factory boot files/links match their pinned hashes and link targets.
- Existing paired `/boot/amp-p029/Image`, `initrd`, and `stage-B.dtb` match their pinned sizes and hashes. No RPMsg device or echo module was present.
- Before staging, `/boot` had 15900672 bytes free and `/boot/amp-p029/initdiag-v1` was absent.

The user then supplied the 1214-line COM5 cold-start transcript. It shows `soc cold boot`, SPL-verified U-Boot prefix `7d8fe670d9` and control-key DT prefix `43164981ef` both `OK`, proper U-Boot `g149b1c5`, and factory/default runtime policy `0`. The unchanged `/boot.scr` starts the original Debian system with root p3; Linux reaches the LubanCat-3-v2 Debian 12 login on kernel `6.1.99-rk3576 #8`. No P030, B, or M0 command appears in the transcript. Because its console capture contains login credentials, only these sanitized facts are recorded here.

## Passive staging result

Only `/boot/amp-p029/initdiag-v1` was added. The staged files and installed readback identities are:

| File | Bytes | SHA256 |
| --- | ---: | --- |
| `amp-signed.itb` | 129024 | `cc1c99d5532e5cc1cd63e690b5d205964ba75a7f68692eff3282f02ec308df30` |
| `stage-B.cmd` | 2990 | `593bf63f7e7ce785de22e9068a37ee75c7f75b3c614b9005ffdf6fd16d24e6f8` |
| `stage-B.scr` | 3062 | `68580407d0b0961e27a8938950aeb42b967e9bd4b534cdd23c1625d4f1d32642` |
| `P030_INITDIAG.json` | 9370 | `81d4c62833ac1ce29c6172b2281e338ef2bdec9310cfc75d1edac62b4146d04f` |
| `SHA256SUMS` | 322 | `5e43ef4e75186e5d6b8e83161091e1d2629802854a9fa06b08a3884b12e91bc1` |
| `INSTALL_RECEIPT.json` | generated on target | `ac30c4157d53bcc627d4d92528051f70d667bb65091bbc10818e467bd14a01ac` |

The final receipt reports result `P030_INITDIAG_PASSIVE_STAGE_RECOVERED_READBACK_PASS`, existing P029 tree fingerprint `4f74b503a8e93b4c053d9b3b914a115bacc4ff43e555c9a7aa2c62f409fde524`, existing files/metadata unchanged, factory files/links unchanged, U-Boot hash matching P030, and no M0 start, KO load, or reboot. The exact manifest SHA is `81d4c62833ac1ce29c6172b2281e338ef2bdec9310cfc75d1edac62b4146d04f`; the final receipt is stored in the ignored local result record.

The first installer invocation stopped before persistent writes on an incorrect sysfs field name. The next invocation passed its gates and atomically committed the five package files, then stopped because its post-commit parent whitelist omitted the new directory. A read-only SSH check confirmed all five exact sizes/hashes and no receipt. The corrected helper revalidated the default boot, full U-Boot hash, factory files, prior P029 inventory, and all five payloads; it created only the missing receipt and removed the exact task RAM source. No pre-existing payload was overwritten.

The tracked installer is [p030_install_initdiag_v1.py](../../../scripts/board/p030_install_initdiag_v1.py). The exact package, original receipt response, and build artifacts are under ignored `artifacts/local/p030-m0-initdiag-v1/`.

## User B attempt and recovery

The user manually ran one P030 initdiag-v1 load/source attempt. The reported COM5 excerpt was:

~~~text
=> load mmc 0:2 0x4c000000 /amp-p029/initdiag-v1/stage-B.scr
3062 bytes read in 8 ms (373 KiB/s)
=> source 0x4c000000
## Executing script at 4c000000
=>
~~~

The user reported no change for 10 seconds and no COM6 output. The host-verified command text begins with an echo P030 banner, but no banner or later script diagnostic appears in the excerpt. This is an inconclusive execution result: the available output cannot establish whether the script body ran or whether amp_m0load was reached. Do not infer either M0-started or M0-not-started. B is not accepted, and no same-session retry, manual amp_m0load, booti, KO, or C stage was reported.

After the user clarified that recovery had just completed, a read-only SSH session confirmed Debian 12, kernel 6.1.99-rk3576 #8, default root=/dev/mmcblk0p3, and no amp_test_stage= argument. On the recovered board, sha256sum -c SHA256SUMS passed for the P030 manifest, FIT, command text, and U-Boot script. No credentials are recorded. This post-recovery hash check confirms the staged files now present; it does not prove what bytes were consumed by the earlier source command.

## Remaining gates

- Review the complete COM5 and COM6 raw captures from the attempt, if available, and explain why the expected first banner was absent before planning another B.
- The agent did not execute source, start M0, load a KO, access MMIO, or reboot.
- Whether the user attempt reached amp_m0load, target hardware FIT verification, M0 checkpoint progression, effective mapping/cache behavior, full B acceptance, Linux HELLO_ACK/PONG, and D remain unverified.

See [P030 B attempt record](P030_INITDIAG_B_EXECUTION.md), [P030 manual B guide](P030_M0_INITDIAG_GUIDE.md), and [machine-readable preparation record](P030_M0_INITDIAG_HOST_PREPARATION.json).
