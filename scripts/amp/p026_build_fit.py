#!/usr/bin/env python3
"""Build an unsigned Host M0 FIT; preserve external data while finalizing size.

Rockchip's pinned partition loader needs root totalsize, unlike generic FIT.
Only the preallocated four-byte property is edited. No board or firmware run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from p026_generate_preload_contract import files


def finalize_totalsize(raw):
    data = bytearray(raw)
    header = struct.unpack_from(">10I", data)
    magic, tree_size, off, strings, _, version, _, _, string_size, struct_size = header
    assert magic == 0xD00DFEED and version >= 17
    assert 40 <= off < off + struct_size <= tree_size <= len(data)
    assert strings + string_size <= tree_size
    assert struct.unpack_from(">I", data, off)[0] == 1
    off += 4
    end = data.index(0, off, tree_size)
    assert end == off, "FIT root must have empty name"
    off = (end + 4) & ~3
    while off < tree_size:
        token = struct.unpack_from(">I", data, off)[0]
        off += 4
        if token == 4:  # FDT_NOP
            continue
        if token != 3:  # Root properties precede first child
            break
        length, nameoff = struct.unpack_from(">2I", data, off)
        off += 8
        assert nameoff < string_size and off + length <= tree_size
        nameend = data.index(0, strings + nameoff, strings + string_size)
        name = bytes(data[strings + nameoff:nameend])
        if name == b"totalsize":
            assert length == 4 and len(data) < 0x80000000
            struct.pack_into(">I", data, off, len(data))
            return bytes(data)
        off = (off + length + 3) & ~3
    raise ValueError("preallocated root totalsize not found")


def fdt_value(path, node, name, kind="x"):
    return subprocess.check_output(["fdtget", "-t", kind, str(path), node, name], text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--contract", type=Path, required=True)
    p.add_argument("--bin", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    generated = files(a.contract.read_bytes())
    a.output.mkdir(parents=True, exist_ok=False)
    for name, content in generated.items():
        (a.output / name).write_text(content)
    shutil.copyfile(a.bin, a.output / "rttmcu.bin")
    command = ["mkimage", "-f", "amp.its", "-E", "-p", "0x1000", "-B", "0x200", "amp-host.itb"]
    build = subprocess.run(command, cwd=a.output, capture_output=True, text=True, check=True)
    (a.output / "mkimage.log").write_text(build.stdout + build.stderr)
    fit = a.output / "amp-host.itb"
    before = fit.read_bytes()
    after = finalize_totalsize(before)
    assert len(after) == len(before)
    fit.write_bytes(after)
    assert int(fdt_value(fit, "/", "totalsize"), 16) == len(after)
    position = int(fdt_value(fit, "/images/mcu", "data-position"), 16)
    size = int(fdt_value(fit, "/images/mcu", "data-size"), 16)
    expected_hash = bytes(int(value, 16) for value in fdt_value(
        fit, "/images/mcu/hash", "value", "bx").split()).hex()
    assert 4096 <= position < position + size <= len(after)
    payload = after[position:position + size]
    assert payload == a.bin.read_bytes()
    assert hashlib.sha256(payload).hexdigest() == expected_hash
    layout = json.loads(generated["layout.json"])
    for prop, expected in (("load", layout["code_pa"]), ("entry", layout["m0_entry"]),
                           ("rockchip,mcu-code-size", layout["code_size"]),
                           ("rockchip,mcu-shared-window-base", layout["window_pa"])):
        assert int(fdt_value(fit, "/images/mcu", prop), 16) == expected
    assert [int(v, 16) for v in fdt_value(fit, "/images/mcu", "rockchip,mcu-shared-region").split()] == [layout["shared_pa"], layout["shared_size"]]
    report = {"evidence": "HOST_TESTED", "status": "UNSIGNED_NOT_DEPLOYABLE",
              "deployment_authorized": False, "board_access": False,
              "command": command, "exit_code": build.returncode,
              "payload_source": str(a.bin), "payload_sha256": hashlib.sha256(payload).hexdigest(),
              "FIT": {"file": str(fit), "sha256": hashlib.sha256(after).hexdigest(), "size": len(after)},
              "contract_sha256": hashlib.sha256(a.contract.read_bytes()).hexdigest(),
              "root_totalsize_finalized": True, "external_payload_identical": True}
    (a.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
