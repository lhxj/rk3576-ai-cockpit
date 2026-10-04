#!/usr/bin/env python3
"""Host-only P029 signing repair; no board access or flashing.

Provision one development AMP configuration key in a copy of the proven
proper U-Boot control DT. Retain the exact proven code and firmware, two
2MiB FIT slots and original trailing 4MiB. Secrets stay in ignored Host output.
The resulting package needs independent review, readback and cold-boot gates.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

from validate_host_proposal import fdt
from generate_host_proposal import layout

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "artifacts/local/p028-uboot-factory-boot"
TOOLS = ROOT / "artifacts/local/p028-build-final-v4/tools"
PROVEN = ROOT / "artifacts/local/p028-primary-linux-only-packet-v1/uboot-P028-linux-only-8MiB-PENDING-APPROVAL.img"
PROVEN_SHA = "9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e"
SOURCE_COMMIT = "f8b4554584dd475ce783c605850c5e883b0a0fd4"
PAYLOAD = ROOT / "artifacts/local/p029-m0-clean-v3/rtthread.bin"
PAYLOAD_SHA = "40ccde0f4ec1c941fcc1acbd1437a8e4a193e89da2db0682981bd5ae224f1727"
SLOT = 0x200000
LOADER_START = 0x1200
AMP_START = 0x1000
KEY_NAME = "p029dev"
NAMES = ("uboot", "atf-1", "atf-2", "atf-3", "optee", "fdt")
CONTRACT = ROOT / "docs/amp/AMP_PLATFORM_CONTRACT.yaml"
CONTRACT_SHA = "216484d601025308498181dfccaf993c81d5101eb4d44d42392c3ac48e871cc7"
TOOL_SHA = {"mkimage": "988a2b46efb8dc21710b82a0b856496dbc13fa2b0fc194f3f947f1ba35a61f83",
            "fit_check_sign": "3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cell(data):
    if len(data) != 4:
        raise ValueError("expected one 32-bit cell")
    return struct.unpack(">I", data)[0]


def run(argv, cwd, log, env=None):
    result = subprocess.run([str(x) for x in argv], cwd=cwd, env=env,
                            capture_output=True, timeout=90)
    log.write_bytes(result.stdout + b"\nSTDERR\n" + result.stderr)
    result.check_returncode()
    return result.stdout


def extract(container, nodes, name):
    props = nodes["/images/" + name]
    start, size = cell(props["data-position"]), cell(props["data-size"])
    if not LOADER_START <= start < start + size <= SLOT:
        raise ValueError("loader payload bounds: " + name)
    data = container[start:start + size]
    if sha(data) != nodes["/images/" + name + "/hash"]["value"].hex():
        raise ValueError("loader payload hash: " + name)
    return data


def property_line(name, value):
    import re
    if not re.fullmatch(r"[A-Za-z0-9_#,+.-]+", name):
        raise ValueError("invalid property name")
    return name + " = [" + " ".join(f"{b:02x}" for b in value) + "];"


def make(output, complete_signed=False):
    output = output.resolve()
    if not output.is_relative_to(ROOT / "artifacts/local") or (output.exists() and not complete_signed):
        raise ValueError("output must be a fresh ignored Host directory")
    if complete_signed and (not output.is_dir() or (output / "result.json").exists()):
        raise ValueError("completion only permits an incomplete signed Host output")
    commit = subprocess.check_output(["git", "-C", SOURCE, "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", SOURCE, "status", "--porcelain"], text=True).strip()
    if commit != SOURCE_COMMIT or dirty:
        raise ValueError("derived U-Boot source identity mismatch")
    if sha(CONTRACT.read_bytes()) != CONTRACT_SHA:
        raise ValueError("canonical contract identity mismatch")
    geometry = layout(json.loads(CONTRACT.read_text()))
    for name, expected in TOOL_SHA.items():
        if sha((TOOLS / name).read_bytes()) != expected:
            raise ValueError("fixed vendor tool identity mismatch: " + name)
    old, m0 = PROVEN.read_bytes(), PAYLOAD.read_bytes()
    if len(old) != 4 * SLOT or sha(old) != PROVEN_SHA or sha(m0) != PAYLOAD_SHA:
        raise ValueError("proven U-Boot or unchanged M0 identity mismatch")
    stack, entry = struct.unpack_from("<II", m0)
    if stack != geometry["code_size"] or not entry & 1 or entry >= geometry["code_size"]:
        raise ValueError("M0 vector outside canonical reserve")
    if old[:SLOT] != old[SLOT:2*SLOT]:
        raise ValueError("proven loader FIT slots differ")
    nodes = fdt(old)
    payloads = {n: extract(old, nodes, n) for n in NAMES}
    dt_nodes = fdt(payloads["fdt"])
    if "/signature" in dt_nodes:
        raise ValueError("unexpected preexisting key; never replace a trust anchor")
    if not complete_signed:
        output.mkdir(mode=0o700)
    secrets = output / "private"
    if not complete_signed:
        secrets.mkdir(mode=0o700)
    key, cert = secrets / (KEY_NAME + ".key"), secrets / (KEY_NAME + ".crt")
    # Child processes inherit a restrictive umask before creating key material.
    if not complete_signed:
        old_mask = os.umask(0o077)
        try:
            run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048",
                 "-out", key], output, output / "keygen.log")
            run(["openssl", "req", "-batch", "-new", "-x509", "-sha256", "-key", key,
                 "-out", cert, "-days", "3650", "-subj", "/CN=P029 development AMP test"],
                output, output / "certificate.log")
        finally:
            os.umask(old_mask)
    if key.stat().st_mode & 0o077:
        raise ValueError("private key permissions too broad")
    control = output / "control-key.dtb"
    if not complete_signed:
        control.write_bytes(payloads["fdt"])
        (output / "rttmcu.bin").write_bytes(m0)
    total = AMP_START + ((len(m0) + 511) & ~511)
    its = f'''/dts-v1/;
/ {{
    description = "P029 development signed cold BUS M0";
    totalsize = <0x{total:x}>;
    #address-cells = <1>;
    images {{ mcu {{
        description = "bus_mcu"; data = /incbin/("rttmcu.bin");
        type = "standalone"; arch = "arm"; compression = "none";
        load = <0x{geometry['code_pa']:x}>; entry = <0x{entry:x}>;
        rockchip,mcu-code-size = <0x{geometry['code_size']:x}>;
        rockchip,mcu-shared-region = <0x{geometry['shared_pa']:x} 0x{geometry['shared_size']:x}>;
        rockchip,mcu-shared-window-base = <0x{geometry['window_pa']:x}>;
        hash {{ algo = "sha256"; }};
    }}; }};
    configurations {{ default = "conf"; conf {{
        loadables = "mcu";
        signature {{ algo = "sha256,rsa2048"; key-name-hint = "{KEY_NAME}";
            sign-images = "loadables"; }};
    }}; }};
}};
'''
    if complete_signed:
        if (output / "amp-signed.its").read_text() != its or (output / "rttmcu.bin").read_bytes() != m0:
            raise ValueError("incomplete output source differs")
    else:
        (output / "amp-signed.its").write_text(its)
    env = dict(os.environ, SOURCE_DATE_EPOCH="1790985600")
    # Vendor -E/-p externalizes BEFORE signing and sets root totalsize before
    # signing. Never patch authenticated properties after this command.
    command = [TOOLS / "mkimage", "-f", "amp-signed.its", "-E", "-p", "0x1000",
               "-k", secrets, "-K", control, "-r", "amp-signed.itb"]
    if not complete_signed:
        run(command, output, output / "amp-sign.log", env)
    fit = (output / "amp-signed.itb").read_bytes()
    fit_nodes = fdt(fit)
    props = fit_nodes["/images/mcu"]
    # Vendor signing may append bounded workspace while expanding the key DT.
    # It authenticates the final root totalsize; do not truncate or rewrite it.
    if (not total <= len(fit) <= total+64*1024 or
        cell(fit_nodes["/"]["totalsize"]) != len(fit) or
        struct.unpack_from(">I", fit, 4)[0] > AMP_START or
        cell(props["data-position"]) != AMP_START or cell(props["data-size"]) != len(m0) or
        fit[AMP_START:AMP_START+len(m0)] != m0 or
        fit_nodes["/images/mcu/hash"]["value"].hex() != PAYLOAD_SHA):
        raise ValueError("signed external FIT geometry/payload mismatch")
    sig = fit_nodes["/configurations/conf/signature"]
    paths = sig["hashed-nodes"].rstrip(b"\0").decode().split("\0")
    if set(paths) != {"/", "/configurations", "/configurations/conf", "/images/mcu", "/images/mcu/hash"} or len(paths) != 5:
        raise ValueError("AMP configuration signature coverage mismatch")
    if len(sig["value"]) != 256:
        raise ValueError("expected RSA2048 signature")
    new_dt = control.read_bytes()
    new_nodes = fdt(new_dt)
    key_path = "/signature/key-" + KEY_NAME
    if (new_nodes[key_path]["required"] != b"conf\0" or
        set(p for p in new_nodes if p.startswith("/signature")) !=
            {"/signature", key_path, key_path+"/hash@np", key_path+"/hash@c"}):
        raise ValueError("expected single required-conf anchor")
    if {p: v for p,v in new_nodes.items() if not p.startswith("/signature")} != dt_nodes:
        raise ValueError("control DT change outside signature subtree")
    run([TOOLS / "fit_check_sign", "-f", output / "amp-signed.itb", "-k", control, "-s"],
        output, output / "amp-verify.log")
    # Recreate only the two FIT slots, preserving proven payload metadata.
    payloads["fdt"] = new_dt
    loader_total = LOADER_START + sum((len(d)+511)&~511 for d in payloads.values())
    if loader_total > SLOT:
        raise ValueError("loader no longer fits proven 2MiB slot")
    lines = ["/dts-v1/;", "/ {"]
    for name, value in nodes["/"].items():
        if name == "timestamp":
            continue
        if name == "totalsize":
            value = struct.pack(">I", loader_total)
        lines.append("    " + property_line(name, value))
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
        if name == "value":
            raise ValueError("proven outer FIT unexpectedly authenticated")
        lines.append("                " + property_line(name, value))
    lines += ["            };", "        };", "    };", "};", ""]
    loader_its = "\n".join(lines)
    if complete_signed and (output / "loader-key.itb").exists():
        if (output / "loader-key.its").read_text() != loader_its:
            raise ValueError("incomplete loader source differs")
    else:
        (output / "loader-key.its").write_text(loader_its)
    loader_env = dict(os.environ, SOURCE_DATE_EPOCH=str(cell(nodes["/"]["timestamp"])))
    # This fixed vendor tool uses IMAGE_ALIGN_SIZE=512 and has no -B option.
    if not (complete_signed and (output / "loader-key.itb").exists()):
        run([TOOLS / "mkimage", "-f", "loader-key.its", "-E", "-p", hex(LOADER_START),
             "loader-key.itb"], output, output / "loader-package.log", loader_env)
    loader = (output / "loader-key.itb").read_bytes()
    loader_nodes = fdt(loader)
    if (not loader_total <= len(loader) <= min(loader_total+64*1024, SLOT) or
        cell(loader_nodes["/"]["totalsize"]) != len(loader)):
        raise ValueError("loader totalsize mismatch")
    preserved = []
    for name in NAMES:
        actual = extract(loader, loader_nodes, name)
        if actual != payloads[name]:
            raise ValueError("loader package content mismatch: " + name)
        for prop,value in nodes["/images/"+name].items():
            if prop not in ("data", "data-size", "data-position", "data-offset") and loader_nodes["/images/"+name][prop] != value:
                raise ValueError("loader firmware metadata changed")
        preserved.append({"name": name, "sha256": sha(actual), "changed": name == "fdt"})
    for path in ("/configurations", "/configurations/conf", "/configurations/conf/signature"):
        if loader_nodes[path] != nodes[path]:
            raise ValueError("loader configuration changed")
    padded = loader + bytes(SLOT-len(loader))
    complete = padded*2 + old[2*SLOT:]
    image = output / "uboot-P029-required-conf-8MiB.img"
    image.write_bytes(complete)
    # Public certificates are safe to hash; never serialize private key bytes.
    public_der = subprocess.check_output(["openssl", "x509", "-in", cert, "-outform", "DER"])
    report = {"status": "HOST_SIGNED_CANDIDATE_PENDING_REVIEW", "board_access": False,
              "source_commit": commit, "proper_uboot_code_changed": False,
              "contract_sha256": CONTRACT_SHA,
              "proven_partition_sha256": PROVEN_SHA, "new_partition_sha256": sha(complete),
              "size": len(complete), "slot_size": SLOT, "copies": 2,
              "preserved_tail_sha256": sha(old[2*SLOT:]), "payloads": preserved,
              "amp_sha256": sha(fit), "amp_size": len(fit), "m0_sha256": PAYLOAD_SHA,
              "control_dt_sha256": sha(new_dt), "control_dt_size": len(new_dt),
              "certificate_der_sha256": sha(public_der), "key_name": KEY_NAME,
              "required": "conf", "signed_nodes": paths,
              "loader_authenticated": False, "amp_config_authenticated": True,
              "trust_scope": "development AMP anchor in manually provisioned proper control DT; no OTP/ROM secure-boot change",
              "private_key": "ignored Host private/ only; never upload or commit",
              "tools": {n: sha((TOOLS/n).read_bytes()) for n in ("mkimage", "fit_check_sign")},
              "runtime_hw_crypto": "UNVERIFIED", "D_ready": False}
    (output / "result.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({k:report[k] for k in ("status", "new_partition_sha256", "amp_sha256", "amp_size", "control_dt_size")}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--complete-signed-output", action="store_true",
                        help="validate and finish an incomplete signed Host output; never re-sign it")
    args = parser.parse_args()
    make(args.output, args.complete_signed_output)
