#!/usr/bin/env python3
"""Host-only final-FDT guard repair package; never signs, flashes or contacts a board.

Replace only proper U-Boot code in the readback-proven signed-v8 container.
Retain its exact public-key control DT, firmware, AMP FIT and trailing 4MiB.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess

from p029_sign_amp import NAMES, SLOT, LOADER_START, TOOLS, TOOL_SHA, extract, property_line, sha
from validate_host_proposal import fdt

ROOT = Path(__file__).resolve().parents[2]
SOURCE_COMMIT = "149b1c53e368a0d77e542cfdf3bed6db3374682a"
BASE_COMMIT = "f8b4554584dd475ce783c605850c5e883b0a0fd4"
BASE = ROOT / "artifacts/local/p029-signed-v8/uboot-P029-required-conf-8MiB.img"
BASE_SHA = "ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4"
CONTROL_SHA = "43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce"
AMP = ROOT / "artifacts/local/p029-signed-v8/amp-signed.itb"
AMP_SHA = "331431fcf1f914c91efd54ffb52a50f9e0ef17ea9df7ba2ed8ef7c675ed6c041"
CONTRACT = ROOT / "docs/amp/AMP_PLATFORM_CONTRACT.yaml"
CONTRACT_SHA = "216484d601025308498181dfccaf993c81d5101eb4d44d42392c3ac48e871cc7"
OLD_BUILD = ROOT / "artifacts/local/p028-build-final-v4"


def require(value, message):
    if not value:
        raise ValueError(message)


def make(source, build, output):
    source, build, output = source.resolve(), build.resolve(), output.resolve()
    for path in (source, build, output):
        require(path.is_relative_to(ROOT / "artifacts/local"), "ignored Host path required")
        require(subprocess.run(["git", "-C", ROOT, "check-ignore", "-q", path]).returncode == 0,
                "Host path must be ignored")
    require(not output.exists(), "fresh output required")
    require(subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD"], text=True).strip() == SOURCE_COMMIT,
            "reviewed source commit required")
    require(subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD^"], text=True).strip() == BASE_COMMIT,
            "proven source predecessor required")
    require(not subprocess.check_output(["git", "-C", source, "status", "--porcelain"], text=True).strip(),
            "dirty source forbidden")
    changed = subprocess.check_output(["git", "-C", source, "diff", "--name-only", "HEAD^", "HEAD"], text=True).splitlines()
    require(changed == ["drivers/cpu/rockchip_amp.c"], "unexpected source scope")
    require(sha(CONTRACT.read_bytes()) == CONTRACT_SHA, "canonical contract changed")
    require((build / ".config").read_bytes() == (OLD_BUILD / ".config").read_bytes(), "proven Kconfig changed")
    require((build / "u-boot.dtb").read_bytes() == (OLD_BUILD / "u-boot.dtb").read_bytes(), "build control DT changed")
    for name, expected in TOOL_SHA.items():
        require(sha((TOOLS / name).read_bytes()) == expected, "fixed vendor tool changed")
    old, amp = BASE.read_bytes(), AMP.read_bytes()
    require(len(old) == 4*SLOT and sha(old) == BASE_SHA and old[:SLOT] == old[SLOT:2*SLOT], "proven signed-v8 slots")
    require(len(amp) == 130560 and sha(amp) == AMP_SHA, "unchanged signed AMP identity")
    nodes = fdt(old)
    previous = {n: extract(old, nodes, n) for n in NAMES}
    require(sha(previous["fdt"]) == CONTROL_SHA, "proven public-key control DT")
    payloads = dict(previous)
    payloads["uboot"] = (build / "u-boot-nodtb.bin").read_bytes()
    require(payloads["uboot"] and payloads["uboot"] != previous["uboot"], "proper code not replaced")
    total = LOADER_START + sum((len(d)+511)&~511 for d in payloads.values())
    require(total <= SLOT, "loader exceeds original slot")
    output.mkdir(mode=0o700)
    lines = ["/dts-v1/;", "/ {"]
    for name, value in nodes["/"].items():
        if name != "timestamp":
            lines.append("    " + property_line(name, struct.pack(">I", total) if name == "totalsize" else value))
    lines.append("    images {")
    for name in NAMES:
        (output / (name + ".bin")).write_bytes(payloads[name])
        lines += [f"        {name} {{", f'            data = /incbin/("{name}.bin");']
        for prop, value in nodes["/images/" + name].items():
            if prop not in ("data", "data-size", "data-position", "data-offset"):
                lines.append("            " + property_line(prop, value))
        lines += ['            hash { algo = "sha256"; };', "        };"]
    lines += ["    };", "    configurations {"]
    for name, value in nodes["/configurations"].items():
        lines.append("        " + property_line(name, value))
    lines.append("        conf {")
    for name, value in nodes["/configurations/conf"].items():
        lines.append("            " + property_line(name, value))
    lines.append("            signature {")
    for name, value in nodes["/configurations/conf/signature"].items():
        require(name != "value", "outer FIT already authenticated; separate signing required")
        lines.append("                " + property_line(name, value))
    lines += ["            };", "        };", "    };", "};", ""]
    (output / "loader.its").write_text("\n".join(lines))
    env = dict(os.environ, SOURCE_DATE_EPOCH=str(struct.unpack(">I", nodes["/"]["timestamp"])[0]))
    result = subprocess.run([TOOLS / "mkimage", "-f", "loader.its", "-E", "-p", hex(LOADER_START), "loader.itb"],
                            cwd=output, env=env, capture_output=True, timeout=90)
    (output / "package.stdout").write_bytes(result.stdout)
    (output / "package.stderr").write_bytes(result.stderr)
    result.check_returncode()
    packed = (output / "loader.itb").read_bytes()
    actual = fdt(packed)
    require(total <= len(packed) <= min(total+65536, SLOT), "packed FIT bounds")
    require(struct.unpack(">I", actual["/"]["totalsize"])[0] == len(packed), "packed root size")
    for name in NAMES:
        require(extract(packed, actual, name) == payloads[name], "packaged payload mismatch: " + name)
        for prop, value in nodes["/images/" + name].items():
            if prop not in ("data", "data-size", "data-position", "data-offset"):
                require(actual["/images/" + name][prop] == value, "firmware metadata changed")
    for path in ("/configurations", "/configurations/conf", "/configurations/conf/signature"):
        require(actual[path] == nodes[path], "loader policy metadata changed")
    padded = packed + bytes(SLOT-len(packed))
    full = padded*2 + old[2*SLOT:]
    (output / "uboot-P030-final-fdt-8MiB.img").write_bytes(full)
    (output / "rollback-P029-signed-v8-8MiB.img").write_bytes(old)
    (output / "amp-signed.itb").write_bytes(amp)
    verify = subprocess.run([TOOLS / "fit_check_sign", "-f", AMP, "-k", output / "fdt.bin", "-s"],
                            capture_output=True, timeout=30)
    (output / "amp-verify.stdout").write_bytes(verify.stdout)
    (output / "amp-verify.stderr").write_bytes(verify.stderr)
    verify.check_returncode()
    report = {"id": "P030_FINAL_FDT_GUARD_REPAIR", "state": "HOST_PACKAGED_PENDING_INDEPENDENT_REVIEW",
              "source_commit": SOURCE_COMMIT, "parent_source_commit": BASE_COMMIT,
              "source_scope": changed, "board_access": False, "size": len(full), "slot_bytes": SLOT,
              "copies": 2, "uboot_sha256": sha(full), "rollback_sha256": sha(old),
              "proper_code_sha256": sha(payloads["uboot"]), "control_dt_sha256": CONTROL_SHA,
              "signed_amp_sha256": AMP_SHA, "contract_sha256": CONTRACT_SHA,
              "tail_4MiB_sha256": sha(old[2*SLOT:]),
              "payloads": [{"name": n, "bytes": len(payloads[n]), "sha256": sha(payloads[n]),
                            "changed": payloads[n] != previous[n]} for n in NAMES],
              "build_ELF_sha256": sha((build / "u-boot").read_bytes()),
              "build_config_sha256": sha((build / ".config").read_bytes()),
              "vendor_software_AMP_verify_exit": verify.returncode,
              "signature_or_private_key_change": False, "OTP_change": False,
              "new_default_linux_cold_boot": "UNVERIFIED", "new_B": "UNVERIFIED", "D_ready": False,
              "trust_limit": "Outer loader remains policy0 SHA container; AMP required-conf anchor and signed FIT retained exactly"}
    (output / "result.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "output"):
        ap.add_argument("--" + name, type=Path, required=True)
    args = ap.parse_args()
    make(args.source, args.build, args.output)
