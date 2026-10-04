#!/usr/bin/env python3
"""Build the additive P030 script-format correction; no board access."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import tarfile
import zlib

from p029_prepare_signed_stage_b import script_text

OLD_SCRIPT_SHA = "68580407d0b0961e27a8938950aeb42b967e9bd4b534cdd23c1625d4f1d32642"
OLD_MANIFEST_SHA = "81d4c62833ac1ce29c6172b2281e338ef2bdec9310cfc75d1edac62b4146d04f"
BASE_INSTALLER_SHA = "db21eba32dfc06a43920ba5ce0c0babdd668d71f323e8f7d85a51fd0d9c85466"
V1_RECEIPT_SHA = "ac30c4157d53bcc627d4d92528051f70d667bb65091bbc10818e467bd14a01ac"
V1_RECEIPT_BYTES = 1741
DESTINATION = "/boot/amp-p029/initdiag-source-fix-v2"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def identity(data):
    return {"size": len(data), "sha256": sha(data)}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old_dir = args.repo / "artifacts/local/p030-m0-initdiag-v1/packet"
    old = (old_dir / "stage-B.scr").read_bytes()
    command = (old_dir / "stage-B.cmd").read_bytes()
    raw_manifest = (old_dir / "P030_INITDIAG.json").read_bytes()
    helper = (args.repo / "scripts/board/p030_install_initdiag_v1.py").read_bytes()
    require(sha(old) == OLD_SCRIPT_SHA and len(old) == 3062, "exact old SCRIPT identity")
    require(sha(raw_manifest) == OLD_MANIFEST_SHA, "exact old manifest identity")
    require(sha(helper) == BASE_INSTALLER_SHA, "reviewed installer base identity")
    header = struct.unpack(">7I4B32s", old[:64])
    header_for_crc = bytearray(old[:64])
    header_for_crc[4:8] = bytes(4)
    require(header[0] == 0x27051956 and header[1] == zlib.crc32(header_for_crc), "old header CRC")
    require(header[3] + 64 == len(old) and header[6] == zlib.crc32(old[64:]), "old data CRC and length")
    require(header[7:11] == (5, 7, 6, 0), "original Linux/PPC/SCRIPT/NONE")
    require(struct.unpack(">II", old[64:72]) == (len(command), 0xffffffff), "exact rejected component table")
    require(old[72:] == command, "old command bytes")
    try:
        script_text(old)
    except ValueError as exc:
        require(str(exc) == "single SCRIPT component", "old artifact rejection reason")
    else:
        raise ValueError("old artifact unexpectedly accepted")

    fixed = bytearray(old)
    fixed[68:72] = bytes(4)
    struct.pack_into(">I", fixed, 24, zlib.crc32(fixed[64:]))
    fixed[4:8] = bytes(4)
    struct.pack_into(">I", fixed, 4, zlib.crc32(fixed[:64]))
    fixed = bytes(fixed)
    require(script_text(fixed) == command, "corrected exact command payload")
    permitted = set(range(4, 8)) | set(range(24, 28)) | set(range(68, 72))
    require(all(i in permitted for i, (a, b) in enumerate(zip(old, fixed)) if a != b),
            "only terminator and CRC fields may change")
    require(command.count(b"amp_m0load /amp-p029/initdiag-v1/amp-signed.itb") == 1,
            "one loader using unchanged v1 FIT dependency")

    spec = copy.deepcopy(json.loads(raw_manifest))
    spec["id"] = "P030_INITDIAG_SOURCE_FIX_V2"
    spec["destination"] = DESTINATION
    spec["files"] = {"stage-B.cmd": identity(command), "stage-B.scr": identity(fixed)}
    spec["correction"] = {
        "original_script": identity(old),
        "component_terminator_before": "0xffffffff",
        "component_terminator_after": "0",
        "changes": "Only second table word and header/data CRC fields",
        "command_text_unchanged": True,
        "signed_fit_dependency": "/boot/amp-p029/initdiag-v1/amp-signed.itb",
        "target_hardware_execution": "UNVERIFIED",
    }
    before = spec["board_precondition"]
    before["existing_amp_p029_top_level"] = sorted(before["existing_amp_p029_top_level"] + ["initdiag-v1"])
    v1_names = ["P030_INITDIAG.json", "SHA256SUMS", "amp-signed.itb", "stage-B.cmd", "stage-B.scr"]
    for name in v1_names:
        before["existing_amp_p029_files"]["initdiag-v1/" + name] = identity((old_dir / name).read_bytes())
    before["existing_amp_p029_files"]["initdiag-v1/INSTALL_RECEIPT.json"] = {
        "size": V1_RECEIPT_BYTES, "sha256": V1_RECEIPT_SHA
    }
    before["existing_nested_members"]["initdiag-v1"] = sorted(v1_names + ["INSTALL_RECEIPT.json"])
    manifest = (json.dumps(spec, sort_keys=True, indent=2) + "\n").encode()

    installer = helper.decode().split("def resume_receipt(", 1)[0]
    installer = installer.replace("P030_INITDIAG.json", "SOURCE_FIX.json")
    installer = installer.replace("initdiag-v1", "initdiag-source-fix-v2")
    installer = installer.replace("P030_M0_INITDIAG_V1", "P030_INITDIAG_SOURCE_FIX_V2")
    old_payloads = 'PAYLOADS = {"amp-signed.itb", "stage-B.cmd", "stage-B.scr"}'
    require(installer.count(old_payloads) == 1, "installer payload anchor")
    installer = installer.replace(old_payloads, 'PAYLOADS = {"stage-B.cmd", "stage-B.scr"}')
    installer, count = re.subn(r'PINNED_MANIFEST_SHA = "[0-9a-f]{64}"',
                             'PINNED_MANIFEST_SHA = "' + sha(manifest) + '"', installer)
    require(count == 1, "installer manifest pin anchor")
    installer = installer.replace("P030_INITDIAG_PASSIVE_STAGE_INSTALLED_READBACK_PASS",
                                  "P030_SOURCE_FIX_V2_PASSIVE_STAGE_READBACK_PASS")
    installer += r"""
if __name__ == "__main__":
    if (len(sys.argv) != 5 or sys.argv[1] != "--approved-p030-source-fix-v2"
            or sys.argv[3] != "--installer-sha"):
        raise SystemExit("STOP explicit source-fix-v2 manifest/installer hashes required")
    install(sys.argv[2], sys.argv[4])
"""
    compile(installer, "install-p030.py", "exec")
    packet = args.output / "packet"
    packet.mkdir(parents=True, exist_ok=False)
    content = {"stage-B.cmd": command, "stage-B.scr": fixed, "SOURCE_FIX.json": manifest}
    sums = "".join(f"{sha(data)}  {name}\n" for name, data in sorted(content.items())).encode()
    content["SHA256SUMS"] = sums
    content["install-p030.py"] = installer.encode()
    for name, data in content.items():
        (packet / name).write_bytes(data)
    archive = args.output / "source-fix-v2.tar.gz"
    with tarfile.open(archive, "w:gz") as tar:
        for path in sorted(packet.iterdir()):
            tar.add(path, arcname=path.name)
    report = {
        "destination": DESTINATION, "board_access": False,
        "script": identity(fixed), "command": identity(command),
        "manifest_sha256": sha(manifest), "installer_sha256": sha(installer.encode()),
        "archive": identity(archive.read_bytes()),
        "exact_cmd_unchanged": True, "M0_FIT_rebuilt": False,
        "source_parser_regression": "Required before deployment"
    }
    (args.output / "build-result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
