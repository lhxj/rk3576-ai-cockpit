#!/usr/bin/env python3
"""Fail closed on mismatched RK3576 M0 FIT, RPMsg and Linux DTS inputs.

Default Linux files are pinned Rockchip *reference* fixtures, not LubanCat DTS.
Pass the actual proposed board DTS/header and a proven SRAM remap base for release.
"""
import argparse
import re
import sys
from pathlib import Path


def required(pattern: str, source: str, label: str) -> str:
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if match is None:
        raise ValueError(f"missing {label}")
    return match.group(1)


def number(value: str) -> int:
    return int(re.sub(r"[ULul()]", "", value), 0)


def region(source: str, label: str) -> tuple[int, int]:
    block = required(rf"{re.escape(label)}\s*:\s*[^{{]+\{{([^}}]+)\}}", source, label)
    cells = required(r"reg\s*=\s*<([^>]+)>", block, f"{label} reg")
    parts = [int(x, 0) for x in cells.split()]
    if len(parts) != 4:
        raise ValueError(f"{label} reg must have 4 cells")
    return (parts[0] << 32) | parts[1], (parts[2] << 32) | parts[3]


def check(ok: bool, message: str, failures: list[str]) -> None:
    print(("PASS " if ok else "FAIL ") + message)
    if not ok:
        failures.append(message)


def main() -> int:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtos", type=Path, default=here.parent.parent / "rtos")
    parser.add_argument("--linux-dts", type=Path,
                        default=here / "fixtures/rk3576-amp-reference.dtsi")
    parser.add_argument("--linux-header", type=Path,
                        default=here / "fixtures/rockchip_rpmsg.h")
    parser.add_argument("--linux-echo-driver", type=Path,
                        default=here / "board/amp_echo_linux/rk3576_amp_echo_test.c")
    parser.add_argument("--sram-remap-base", type=lambda x: int(x, 0),
                        help="verified SYS_SGRF_SOC_CON17[31:10]<<10 value")
    args = parser.parse_args()
    rtos = args.rtos / "bsp/rockchip/rk3576-mcu"
    its = (rtos / "Image/amp.its").read_text()
    # SCons preprocesses gcc_link.ld.S; this is the effective linker input.
    linker = (rtos / "gcc_link.ld").read_text()
    echo = (rtos / "applications/amp_echo.c").read_text()
    port = (args.rtos / "bsp/rockchip/common/drivers/rpmsg-lite/lib/rpmsg_lite/porting/platform/RK3576/rpmsg_platform.c").read_text()
    hal = (args.rtos / "bsp/rockchip/common/hal/middleware/rpmsg-lite/lib/include/platform/RK3576/rpmsg_platform.h").read_text()
    config = (args.rtos / "bsp/rockchip/common/hal/middleware/rpmsg-lite/lib/include/platform/RK3576/rpmsg_config.h").read_text()
    dts = args.linux_dts.read_text()
    header = args.linux_header.read_text()
    driver = args.linux_echo_driver.read_text()
    failures: list[str] = []

    fit_load = number(required(r"load\s*=\s*<(0x[0-9a-fA-F]+)>", its, "FIT load"))
    m0_base = number(required(r"LINUX_RPMSG\s*\(rxw\).*?ORIGIN\s*=\s*(0x[0-9a-fA-F]+)", linker, "M0 RPMsg base"))
    linux_base, linux_size = region(dts, "rpmsg_reserved")
    dma_base, dma_size = region(dts, "rpmsg_dma_reserved")
    mcu_base, mcu_size = region(dts, "mcu_reserved")
    link = number(required(r"rockchip,link-id\s*=\s*<(0x[0-9a-fA-F]+)>", dts, "Linux link-id"))
    echo_remote = int(required(r"AMP_ECHO_LINK_ID\s+RL_PLATFORM_SET_LINK_ID\(0U,\s*(\d+)U\)", echo, "M0 link-id"))
    linux_mboxes = required(r"mboxes\s*=\s*<([^>]+)>", dts, "Linux mboxes")
    service_m0 = required(r'#define AMP_ECHO_SERVICE\s+"([^"]+)"', echo, "M0 service")
    service_linux = required(r'#define AMP_ECHO_SERVICE\s+"([^"]+)"', driver, "Linux service")
    patova_offset = number(required(r"addr\s*-\=\s*(0x[0-9a-fA-F]+)\s*;", port,
                                    "M0 platform_patova offset"))

    check(link == echo_remote, f"link-id Linux={link:#x}, M0={echo_remote:#x}", failures)
    check("&mailbox0 0" in linux_mboxes and f"&mailbox{echo_remote} 0" in linux_mboxes,
          f"mailboxes Linux={linux_mboxes.strip()}; expected mailbox0/mailbox{echo_remote}", failures)
    check(f'"mbox-clr{echo_remote}"' in port, f"M0 mailbox{echo_remote} IRQ client", failures)
    check(service_m0 == service_linux, f"NS service {service_m0}/{service_linux}", failures)
    check(fit_load >= mcu_base and fit_load + (512 * 1024) <= mcu_base + mcu_size,
          f"FIT max 512 KiB within MCU reserve {mcu_base:#x}+{mcu_size:#x}", failures)
    check(fit_load + (512 * 1024) <= linux_base or linux_base + linux_size <= fit_load,
          f"FIT {fit_load:#x}+0x80000 disjoint from Linux RPMsg {linux_base:#x}+{linux_size:#x}", failures)
    m0_dma_base = dma_base - patova_offset
    check(0x20000000 <= m0_dma_base and m0_dma_base + dma_size <= 0x40000000,
          f"DMA pool M0 window {m0_dma_base:#x}+{dma_size:#x}", failures)
    check(dma_base + dma_size <= linux_base or linux_base + linux_size <= dma_base,
          "DMA pool disjoint from Linux RPMsg vring reserve", failures)

    m0_values: dict[str, int] = {}
    for m0_macro, linux_macro in (("VRING_SIZE", "RPMSG_VRING_SIZE"),
                                  ("VRING_ALIGN", "RPMSG_VRING_ALIGN"),
                                  ("RL_BUFFER_COUNT", "RPMSG_BUF_COUNT"),
                                  ("RL_BUFFER_PAYLOAD_SIZE", "RPMSG_BUF_PAYLOAD_SIZE")):
        m0_value = number(required(rf"#define\s+{m0_macro}\s+\(?([0-9xa-fA-F]+[ULul]*)\)?", hal if m0_macro.startswith("VRING") else config, m0_macro))
        linux_value = number(required(rf"#define\s+{linux_macro}\s+\(?([0-9xa-fA-F]+[ULul]*)\)?", header, linux_macro))
        m0_values[m0_macro] = m0_value
        check(m0_value == linux_value, f"{m0_macro}/{linux_macro}: {m0_value}/{linux_value}", failures)
    slot_size = m0_values["RL_BUFFER_PAYLOAD_SIZE"] + 16
    required_pool_size = 2 * m0_values["RL_BUFFER_COUNT"] * slot_size
    check(dma_size >= required_pool_size,
          f"RPMsg DMA pool {dma_base:#x}+{dma_size:#x} holds {required_pool_size:#x} buffer bytes", failures)

    if args.sram_remap_base is None:
        check(False, "vring0/1: SYS_SGRF_SOC_CON17 SRAM remap base unproven", failures)
        check(False, f"DMA buffer: M0 {m0_dma_base:#x} -> Linux {dma_base:#x} remap unproven", failures)
    else:
        for index in (0, 1):
            physical = m0_base + index * m0_values["VRING_SIZE"] - 0x20000000 + args.sram_remap_base
            expected = linux_base + index * m0_values["VRING_SIZE"]
            check(physical == expected,
                  f"vring{index}: M0 maps to Linux PA {physical:#x}, DTS {expected:#x}", failures)
        physical_dma = m0_dma_base - 0x20000000 + args.sram_remap_base
        check(physical_dma == dma_base,
              f"DMA buffer: M0 maps to Linux PA {physical_dma:#x}, DTS {dma_base:#x}", failures)
    print(f"RESULT: {'BLOCKED' if failures else 'PASS'} ({len(failures)} failures)")
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as exc:
        print(f"RESULT: BLOCKED ({exc})", file=sys.stderr)
        sys.exit(2)
