#!/usr/bin/env python3
"""Verify fixed Host files and selected CON17 paths. Never access hardware.

This is a bounded byte/source check, not a complete firmware control-flow proof.
It deliberately reports the current register value as unknown.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys


UBOOT_COMMIT = "8f53f800da2c25d0c6ba414fb45902a01675703a"
BL31_SHA = "1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07"
UBOOT_SHA = "c084f257e761b898c45d681a2d1b739ca2cdd99b3916b64f3e9b34109d7a865d"
BL31_LOAD = 0x40040000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def checked_bytes(path, expected_sha):
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == expected_sha,
            f"Unexpected firmware hash: {path.name}")
    return data


def git(root, *args):
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True).strip()


def verify(root, bl31_path, uboot_path):
    require(git(root, "rev-parse", "HEAD") == UBOOT_COMMIT,
            "Unexpected U-Boot source commit")
    require(not git(root, "status", "--porcelain", "--untracked-files=no"),
            "U-Boot tracked source files have local changes")
    bl31 = checked_bytes(bl31_path, BL31_SHA)
    uboot = checked_bytes(uboot_path, UBOOT_SHA)

    def word(va):
        require(BL31_LOAD <= va <= BL31_LOAD + len(bl31) - 4,
                f"Address outside input BL31 payload: {va:#x}")
        return struct.unpack_from("<I", bl31, va - BL31_LOAD)[0]

    def expect(va, instruction):
        require(word(va) == instruction, f"Instruction differs at {va:#x}")

    def immediate(va):
        ins = word(va)
        require(ins & 0x7F800000 == 0x52800000, "Expected MOVZ")
        return ((ins >> 5) & 0xFFFF) << (((ins >> 21) & 3) * 16)

    def keep(va, value):
        ins = word(va)
        require(ins & 0x7F800000 == 0x72800000, "Expected MOVK")
        shift = ((ins >> 21) & 3) * 16
        return (value & ~(0xFFFF << shift)) | (((ins >> 5) & 0xFFFF) << shift)

    def store(va, base):
        ins = word(va)
        require(ins & 0xFFC00000 == 0xB9000000, "Expected STR W unsigned offset")
        require((ins >> 5) & 31 == 0, "Expected x0 base")
        return base + ((ins >> 10) & 0xFFF) * 4

    def branch(va):
        ins = word(va)
        require(ins & 0x7C000000 == 0x14000000, "Expected B/BL")
        offset = ins & 0x3FFFFFF
        if offset & (1 << 25):
            offset -= 1 << 26
        return va + offset * 4

    # SMC argument flow, dispatcher and setter.
    for va, ins in {
        0x4005EB28: 0x2A0003E5, 0x4005EB2C: 0xAA0103E0,
        0x4005EB38: 0xAA0203E1, 0x4005EB40: 0xAA0303E2,
        0x4005EBBC: 0x540006E0, 0x4005EC98: 0x35000160,
        0x4005EC9C: 0x7100043F, 0x4005ECA4: 0x71000C3F,
        0x4005ECA8: 0x540002E0, 0x4005ED04: 0xF240245F,
        0x4005ED08: 0x540000A1, 0x4005ED14: 0xB9000002,
        0x4005ED18: 0x17FFFFF9, 0x4005ECFC: 0xD2800000,
        0x4005ED1C: 0x92800060,
    }.items():
        expect(va, ins)
    require(keep(0x4005EBB4, immediate(0x4005EBB0)) == 0x82000028,
            "MCU service ID differs")
    base = keep(0x4005ECE4, immediate(0x4005ECDC))
    require([store(va, base) for va in (0x4005ECF0, 0x4005ECF4, 0x4005ECF8)]
            == [0x26004058, 0x2600405C, 0x26004060], "Code selector stores differ")
    shared_reg = store(0x4005ED14, keep(0x4005ED10, immediate(0x4005ED0C)))
    require(shared_reg == 0x26004064, "Shared selector address differs")

    # The table row is a register range and saved-state pointer, not a reset value.
    row_va = 0x40069A50
    row = struct.unpack_from("<IIIIQ", bl31, row_va - BL31_LOAD)
    require(row == (0x2600403C, 0x26004064, 4, 0, 0), "Register table row differs")
    require((row_va - 0x40069930) // 24 == 12, "Table row index differs")
    require(immediate(0x4005A3E8) == 47, "Table count differs")
    for va, ins in {
        0x4005A3F0: 0x9124C000, 0x40059C68: 0x9124C000,
        0x40059C94: 0x9124C000, 0x40055498: 0xF90008A3,
        0x40055500: 0xB94000A6, 0x40055504: 0xF9400805,
        0x40055508: 0xB82468A6, 0x4005554C: 0xF9400805,
        0x40055554: 0xB86468A5, 0x4005555C: 0x2A0600A5,
        0x40055564: 0xB90000C5,
    }.items():
        expect(va, ins)
    require(branch(0x4005A3F4) == 0x40055424, "Table setup call differs")
    require(branch(0x40059C6C) == 0x400554CC, "Save path differs")
    require(branch(0x40059C98) == 0x4005551C, "Restore path differs")

    soc = (root / "arch/arm/mach-rockchip/rk3576/rk3576.c").read_text()
    release = soc[soc.index("int fit_standalone_release("):soc.index("#ifndef CONFIG_TPL_BUILD")]
    calls = re.findall(r"sip_smc_mcu_config\s*\(([^;]+)\);", release)
    require(len(calls) == 1 and "MCU_CODE_START_ADDR" in calls[0]
            and "entry_point" in calls[0], "RK3576 MCU caller differs")
    require("MCU_SRAM_START_ADDR" not in release, "Unexpected shared config caller")
    require(release.index("sip_smc_mcu_config") < release.index("TOP_CRU_GATE_CON19")
            < release.index("TOP_CRU_SOFTRST_CON19"), "Release ordering differs")
    loader = (root / "drivers/cpu/rockchip_amp.c").read_text()
    require('entry = load = fit_get_u32_default(fit, noffset, "load"' in loader
            and "standalone_handler(desc, load, data_size);" in loader,
            "Standalone address source differs")
    require('part_get_info_by_name(dev_desc, AMP_PART, &part)' in loader,
            "Partition lookup differs")
    source_hits = git(root, "grep", "-n", "-e", "MCU_SRAM_START_ADDR",
                      "--", "*.c", "*.h").splitlines()
    require(len(source_hits) == 1 and "#define" in source_hits[0],
            "Shared selector now has additional source references; review required")
    needles = [b"bus_mcu", b"Handle standalone:", b"Brought up amps",
               b"Load loadables, ret=%d", b"standalone:"]
    return {
        "uboot_commit": UBOOT_COMMIT, "bl31_payload_sha256": BL31_SHA,
        "uboot_payload_sha256": UBOOT_SHA,
        "con17_setter": {"smc_id": "0x82000028", "mcu_id": 0, "selector": 3,
                         "store_address": hex(shared_reg), "value_source": "SMC x3 low32",
                         "alignment": 1024, "runtime_invoked": False},
        "register_save_restore": {"table": "0x40069930", "rows": 47,
                                  "row": hex(row_va), "start": hex(row[0]),
                                  "end_inclusive": hex(row[1]), "stride": row[2],
                                  "or_mask": row[3], "file_saved_buffer_pointer": row[4],
                                  "is_con17_reset_value": False},
        "uboot_rk3576_shared_selector_call_present": False,
        "uboot_amp_strings_offsets": {v.decode(): uboot.find(v) for v in needles},
        "current_uboot_config_amp": "UNVERIFIED",
        "current_con17": None, "effective_m0_mapping": "UNRESOLVED",
        "board_access": False, "firmware_executed": False,
        "scope": "selected verified instructions and fixed source; not whole-program proof",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uboot-source", type=Path, required=True)
    parser.add_argument("--bl31-bin", type=Path, required=True)
    parser.add_argument("--uboot-bin", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify(args.uboot_source, args.bl31_bin, args.uboot_bin)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
