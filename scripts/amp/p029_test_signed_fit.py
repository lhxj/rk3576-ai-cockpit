#!/usr/bin/env python3
"""Real P029 vendor signature tests; fresh ignored Host fixtures only.

No board access, firmware execution, key provisioning or input modification.
The vendor checker uses software RSA. Independent key mathematics additionally
check hardware serialization, but neither test proves target hardware RSA.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess

from generate_host_proposal import layout, number

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "artifacts/local/p028-uboot-factory-boot"
SOURCE_SHA = "f8b4554584dd475ce783c605850c5e883b0a0fd4"
TOOLS = ROOT / "artifacts/local/p028-build-final-v4/tools"
TOOL_SHA = {
    "mkimage": "988a2b46efb8dc21710b82a0b856496dbc13fa2b0fc194f3f947f1ba35a61f83",
    "fit_check_sign": "3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def need(condition, message):
    if not condition:
        raise ValueError(message)


class Tree:
    """Bounded FDT parser with length-preserving mutation offsets.

    Do not serialize a mutated FIT with dtc/fdtput: it would lose the external
    payload and produce an unrelated failure. All fixtures retain file size.
    """
    def __init__(self, data):
        self.data = bytes(data)
        need(len(data) >= 40, "FDT short header")
        h = struct.unpack_from(">10I", data)
        magic, total, off_s, off_t, off_r, version, _, self.bootcpu, size_t, size_s = h
        need(magic == 0xd00dfeed and version >= 17, "FDT header")
        need(40 <= total <= len(data), "FDT total bounds")
        need(40 <= off_s <= off_s + size_s <= total, "FDT structure bounds")
        need(40 <= off_t <= off_t + size_t <= total, "FDT strings bounds")
        need(40 <= off_r < total and off_r % 8 == 0, "FDT reserve bounds")
        self.total = total
        self.reservations = []
        pos = off_r
        while True:
            need(pos + 16 <= total, "FDT reserve terminator")
            addr, size = struct.unpack_from(">QQ", data, pos)
            pos += 16
            if addr == size == 0:
                break
            self.reservations.append((addr, size))
        self.nodes, self.props, self.names = {}, {}, {}
        stack, pos, end = [], off_s, off_s + size_s
        while pos + 4 <= end:
            start = pos
            token = struct.unpack_from(">I", data, pos)[0]
            pos += 4
            if token == 1:
                nul = data.find(b"\0", pos, end)
                need(nul >= pos, "FDT node terminator")
                name = data[pos:nul].decode("ascii")
                need("/" not in name, "FDT node name")
                path = "/" if not stack else stack[-1].rstrip("/") + "/" + name
                need(path not in self.nodes and (stack or not name), "FDT unique root/node")
                self.nodes[path] = {}
                self.names[path] = (pos, nul - pos)
                stack.append(path)
                pos = (nul + 4) & ~3
            elif token == 3:
                need(stack and pos + 8 <= end, "FDT property header")
                length, offset = struct.unpack_from(">II", data, pos)
                pos += 8
                need(offset < size_t and pos + length <= end, "FDT property bounds")
                nul = data.find(b"\0", off_t + offset, off_t + size_t)
                need(nul >= off_t + offset, "FDT property name")
                name = data[off_t+offset:nul].decode("ascii")
                path = stack[-1]
                need(name not in self.nodes[path], "FDT duplicate property")
                finish = (pos + length + 3) & ~3
                need(finish <= end, "FDT property padding")
                self.nodes[path][name] = data[pos:pos+length]
                self.props[(path, name)] = (start, pos, length, finish)
                pos = finish
            elif token == 2:
                need(stack, "FDT node stack")
                stack.pop()
            elif token == 4:
                pass
            elif token == 9:
                need(not stack and "/" in self.nodes, "FDT root/end")
                break
            else:
                raise ValueError("FDT invalid token")
        else:
            raise ValueError("FDT missing end")

    def replace(self, path, name, value):
        start, pos, length, end = self.props[(path, name)]
        need((len(value)+3)//4 == (length+3)//4, "mutation must preserve padded length")
        result = bytearray(self.data)
        struct.pack_into(">I", result, start + 4, len(value))
        result[pos:end] = value + bytes(end-pos-len(value))
        return bytes(result)

    def remove(self, path, name):
        start, _, _, end = self.props[(path, name)]
        result = bytearray(self.data)
        result[start:end] = struct.pack(">I", 4) * ((end-start)//4)
        return bytes(result)

    def hide(self, path):
        pos, length = self.names[path]
        result = bytearray(self.data)
        result[pos:pos+length] = b"x" * length
        return bytes(result)


def u32(value):
    need(len(value) == 4, "one FDT cell required")
    return struct.unpack(">I", value)[0]


def strings(value):
    need(value.endswith(b"\0"), "terminated string list")
    return value[:-1].decode("ascii").split("\0")


def key_path(tree):
    names = [p for p in tree.nodes if p.startswith("/signature/key-") and p.count("/") == 2]
    need(len(names) == 1, "exactly one trust anchor required")
    return names[0]


def rsa_props(n, e):
    need(n.bit_length() == 2048 and n & 1 and e == 65537, "RSA2048/exponent contract")
    bignums = {"rsa,modulus": n, "rsa,exponent-BN": e,
               "rsa,r-squared": pow(2, 4096, n), "rsa,c": pow(2, 4100, n),
               "rsa,np": (1 << 2180) // n}
    result = {k: v.to_bytes(256, "big") for k, v in bignums.items()}
    result["rsa,exponent"] = e.to_bytes(8, "big")
    result["rsa,n0-inverse"] = ((-pow(n, -1, 1 << 32)) % (1 << 32)).to_bytes(4, "big")
    result["rsa,num-bits"] = (2048).to_bytes(4, "big")
    return result


def key_hashes(props, config):
    sizes = []
    for name in ("N", "E", "C"):
        match = re.search(r"^CONFIG_RSA_" + name + r"_SIZE=(0x[0-9a-fA-F]+|[0-9]+)$", config, re.M)
        need(match is not None, "RSA configuration sizes")
        sizes.append(int(match.group(1), 0))
    ns, es, cs = sizes
    need(ns + es + cs <= 3 * 256 and all(s > 0 and s % 4 == 0 for s in sizes), "RSA hash layout bounds")
    data = bytearray(3 * 256)

    def copy_words(name, offset, length):
        words = struct.unpack(">64I", props[name])
        count = min(256, length) // 4
        data[offset:offset+count*4] = struct.pack("<" + "I"*count, *words[-count:][::-1])

    copy_words("rsa,modulus", 0, ns)
    copy_words("rsa,exponent-BN", ns, es)
    copy_words("rsa,c", ns+es, cs)
    c_hash = hashlib.sha256(data).digest()
    copy_words("rsa,np", ns+es, cs)
    return c_hash, hashlib.sha256(data[:ns+es+cs]).digest()


def audit_key(data, config):
    tree = Tree(data)
    path = key_path(tree)
    key = tree.nodes[path]
    need(tree.nodes["/signature"] == {}, "unexpected signature-root policy/provisioning property")
    need(key["required"] == b"conf\0", "anchor required-conf policy")
    need(key["algo"] == b"sha256,rsa2048\0", "anchor algorithm")
    need(key["key-name-hint"] == path.split("key-", 1)[1].encode() + b"\0", "anchor hint")
    n, e = int.from_bytes(key["rsa,modulus"], "big"), int.from_bytes(key["rsa,exponent"], "big")
    expect = rsa_props(n, e)
    for name, value in expect.items():
        need(key[name] == value, "key factor/length mismatch: " + name)
    c_hash, np_hash = key_hashes(expect, config)
    for child, value in (("hash@c", c_hash), ("hash@np", np_hash)):
        need(tree.nodes[path+"/"+child] == {"algo": b"sha256\0", "value": value}, "hardware key hash: " + child)
    need(set(p for p in tree.nodes if p.startswith("/signature")) ==
         {"/signature", path, path+"/hash@c", path+"/hash@np"}, "trust anchor subtree")
    return n, e, path


def audit_fit(data, contract, payload):
    tree = Tree(data)
    need(u32(tree.nodes["/"]["totalsize"]) == len(data), "signed root totalsize")
    need(tree.nodes["/configurations"]["default"] == b"conf\0", "default config")
    need(tree.nodes["/configurations/conf"]["loadables"] == b"mcu\0", "only MCU loadable")
    need(set(p for p in tree.nodes if p.startswith("/images")) == {"/images", "/images/mcu", "/images/mcu/hash"}, "single MCU image")
    sig = tree.nodes["/configurations/conf/signature"]
    need(sig["algo"] == b"sha256,rsa2048\0" and len(sig["value"]) == 256, "signature algo/size")
    need(sig["sign-images"] == b"loadables\0", "explicit loadables signing")
    expected_paths = {"/", "/configurations", "/configurations/conf", "/images/mcu", "/images/mcu/hash"}
    paths = strings(sig["hashed-nodes"])
    need(len(paths) == 5 and set(paths) == expected_paths, "signed node coverage")
    props, x = tree.nodes["/images/mcu"], layout(contract)
    expect = {"load": x["code_pa"], "entry": number(contract["host_proposal"]["m0_entry"]),
              "rockchip,mcu-code-size": x["code_size"], "rockchip,mcu-shared-window-base": x["window_pa"]}
    for name, value in expect.items():
        need(u32(props[name]) == value, "FIT contract: " + name)
    need(props["rockchip,mcu-shared-region"] == struct.pack(">II", x["shared_pa"], x["shared_size"]), "shared contract")
    need(props["arch"] == b"arm\0" and props["type"] == b"standalone\0" and props["compression"] == b"none\0", "MCU image type")
    start, size = u32(props["data-position"]), u32(props["data-size"])
    need(tree.total <= start < start + size <= len(data), "external payload bounds")
    need(data[start:start+size] == payload, "exact unchanged MCU payload")
    need(tree.nodes["/images/mcu/hash"] == {"algo": b"sha256\0", "value": hashlib.sha256(payload).digest()}, "MCU payload hash")
    return paths


def public_numbers(der):
    """Read PKCS#1 RSA public DER; this function never reads a private key."""
    def tlv(pos, tag):
        need(pos + 2 <= len(der) and der[pos] == tag, "public DER tag")
        size, pos = der[pos+1], pos+2
        if size & 128:
            count = size & 127
            need(0 < count <= 4 and pos+count <= len(der), "public DER size")
            size = int.from_bytes(der[pos:pos+count], "big")
            pos += count
        need(pos+size <= len(der), "public DER bounds")
        return pos, pos+size
    pos, end = tlv(0, 0x30)
    np, ne = tlv(pos, 2)
    ep, ee = tlv(ne, 2)
    need(ee == end == len(der) and der[np] < 128 and der[ep] < 128, "RSA public DER sequence")
    return int.from_bytes(der[np:ne], "big"), int.from_bytes(der[ep:ee], "big")


def rejected(function, *args):
    try:
        function(*args)
    except (ValueError, KeyError, UnicodeError, struct.error):
        return True
    return False


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fit", type=Path, required=True)
    p.add_argument("--control", type=Path, required=True)
    p.add_argument("--certificate", type=Path, required=True, help="public X509 certificate only")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--source", type=Path, default=SOURCE)
    p.add_argument("--tools", type=Path, default=TOOLS)
    p.add_argument("--config", type=Path, default=ROOT / "artifacts/local/p028-build-final-v4/.config")
    p.add_argument("--contract", type=Path, default=ROOT / "docs/amp/AMP_PLATFORM_CONTRACT.yaml")
    p.add_argument("--unsigned-fit", type=Path, default=ROOT / "artifacts/local/p029-fit-clean-v3/amp-host.itb")
    p.add_argument("--original-control", type=Path, default=ROOT / "artifacts/local/p028-primary-clean-v1/u-boot.dtb")
    p.add_argument("--payload", type=Path, default=ROOT / "artifacts/local/p029-m0-clean-v3/rtthread.bin")
    args = p.parse_args()
    out = args.output.resolve()
    need(out.is_relative_to(ROOT / "artifacts/local") and not out.exists(), "fresh ignored output required")
    need(subprocess.run(["git", "-C", ROOT, "check-ignore", "-q", out], check=False).returncode == 0, "output must be git ignored")
    commit = subprocess.check_output(["git", "-C", args.source, "rev-parse", "HEAD"], text=True).strip()
    need(commit == SOURCE_SHA, "vendor source identity")
    need(not subprocess.check_output(["git", "-C", args.source, "status", "--porcelain"], text=True).strip(), "vendor source must be clean")
    for name, digest in TOOL_SHA.items():
        need(sha((args.tools/name).read_bytes()) == digest, "vendor tool identity: " + name)
    fit, key, payload = args.fit.read_bytes(), args.control.read_bytes(), args.payload.read_bytes()
    config, contract = args.config.read_text(), json.loads(args.contract.read_text())
    n, e, kp = audit_key(key, config)
    paths = audit_fit(fit, contract, payload)
    ft, kt, old = Tree(fit), Tree(key), Tree(args.original_control.read_bytes())
    need({k:v for k,v in kt.nodes.items() if not k.startswith("/signature")} == old.nodes, "control DT changed outside anchor")
    need(kt.reservations == old.reservations and kt.bootcpu == old.bootcpu, "control DT reservation/boot CPU changed")
    public_pem = subprocess.check_output(["openssl", "x509", "-in", args.certificate, "-pubkey", "-noout"])
    der = subprocess.run(["openssl", "rsa", "-pubin", "-RSAPublicKey_out", "-outform", "DER"], input=public_pem, capture_output=True, check=True).stdout
    need(public_numbers(der) == (n, e), "certificate and trusted DT public key mismatch")
    out.mkdir(mode=0o700)
    cases = []
    tool = args.tools / "fit_check_sign"

    def case(name, image, control, expected, spl=True, guard=None):
        image_path, key_file = out / (name+".itb"), out / (name+".dtb")
        image_path.write_bytes(image)
        key_file.write_bytes(control)
        command = [str(tool), "-f", str(image_path), "-k", str(key_file)] + (["-s"] if spl else [])
        try:
            r = subprocess.run(command, capture_output=True, timeout=20)
            need(len(r.stdout) + len(r.stderr) <= 128*1024, "unexpected unbounded checker log")
            (out / (name+".stdout")).write_bytes(r.stdout)
            (out / (name+".stderr")).write_bytes(r.stderr)
            ok = (r.returncode == 0) if expected == "PASS" else (r.returncode > 0)
            entry = {"name": name, "expected": expected, "exit_code": r.returncode,
                     "pass": ok, "spl_image_list": spl, "fit_sha256": sha(image), "key_dt_sha256": sha(control),
                     "stdout_sha256": sha(r.stdout), "stderr_sha256": sha(r.stderr)}
        except subprocess.TimeoutExpired:
            entry = {"name": name, "expected": expected, "pass": False, "timeout": True}
        if guard is not None:
            entry["independent_guard_rejects"] = guard
            entry["pass"] = entry["pass"] and guard
        cases.append(entry)

    def flip_prop(data, path, name, index=0):
        t = Tree(data)
        value = bytearray(t.nodes[path][name])
        value[index] ^= 1
        return t.replace(path, name, bytes(value))

    case("valid_signed_loadable", fit, key, "PASS")
    case("old_unsigned_required_conf", args.unsigned_fit.read_bytes(), key, "REJECT")
    case("missing_key_block", fit, args.original_control.read_bytes(), "REJECT")
    image_key = kt.replace(kp, "required", b"image\0")
    case("required_image_no_image_sig", fit, image_key, "REJECT", guard=rejected(audit_key, image_key, config))
    for name, altered in (("removed_required", kt.remove(kp, "required")),
                          ("nonrequired_anchor", kt.replace(kp, "required", b"none\0"))):
        case(name, fit, altered, "PASS", guard=rejected(audit_key, altered, config))
    start = u32(ft.nodes["/images/mcu"]["data-position"])
    altered = bytearray(fit)
    altered[start] ^= 1
    corrupt = bytes(altered)
    case("bad_payload_strict", corrupt, key, "REJECT")
    case("bad_payload_default_blindspot", corrupt, key, "PASS", spl=False,
         guard=rejected(audit_fit, corrupt, contract, payload))
    size = u32(ft.nodes["/images/mcu"]["data-size"])
    forged = Tree(corrupt).replace("/images/mcu/hash", "value", hashlib.sha256(corrupt[start:start+size]).digest())
    case("bad_payload_recomputed_hash", forged, key, "REJECT")
    case("bad_config_signature", flip_prop(fit, "/configurations/conf/signature", "value"), key, "REJECT")
    case("missing_config_signature", ft.hide("/configurations/conf/signature"), key, "REJECT")
    for name in ("load", "entry", "rockchip,mcu-code-size", "rockchip,mcu-shared-window-base", "data-position", "data-size"):
        altered = flip_prop(fit, "/images/mcu", name, -1)
        case("changed_"+name.replace(",", "_").replace("-", "_"), altered, key, "REJECT")
    for index, name in ((3, "shared_base"), (7, "shared_size")):
        case("changed_"+name, flip_prop(fit, "/images/mcu", "rockchip,mcu-shared-region", index), key, "REJECT")
    case("changed_payload_hash", flip_prop(fit, "/images/mcu/hash", "value"), key, "REJECT")
    case("changed_root_totalsize", flip_prop(fit, "/", "totalsize", -1), key, "REJECT")
    case("changed_config_loadables", flip_prop(fit, "/configurations/conf", "loadables"), key, "REJECT")
    for name in ("rsa,np", "rsa,exponent-BN", "rsa,c"):
        altered = flip_prop(key, kp, name, -1)
        case("wrong_hw_"+name.replace(",", "_").replace("-", "_"), fit, altered, "PASS",
             guard=rejected(audit_key, altered, config))
    for name in ("rsa,r-squared", "rsa,n0-inverse"):
        altered = flip_prop(key, kp, name, -1)
        case("wrong_sw_"+name.replace(",", "_").replace("-", "_"), fit, altered, "REJECT",
             guard=rejected(audit_key, altered, config))
    # Independent real RSA key, not a stub or a read of the supplied private key.
    private = out / "wrong-independent.key"
    fd = os.open(private, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.close(fd)
    subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", private], capture_output=True, check=True, timeout=30)
    need(stat.S_IMODE(private.stat().st_mode) == 0o600, "wrong private key permissions")
    wrong_der = subprocess.run(["openssl", "rsa", "-in", private, "-RSAPublicKey_out", "-outform", "DER"], capture_output=True, check=True).stdout
    wn, we = public_numbers(wrong_der)
    wp = rsa_props(wn, we)
    wrong_key = key
    for name, value in wp.items():
        wrong_key = Tree(wrong_key).replace(kp, name, value)
    wc, wnp = key_hashes(wp, config)
    for child, value in (("hash@c", wc), ("hash@np", wnp)):
        wrong_key = Tree(wrong_key).replace(kp+"/"+child, "value", value)
    audit_key(wrong_key, config)
    case("independent_wrong_public_key", fit, wrong_key, "REJECT")
    bad_fit, bad_key = bytearray(fit), bytearray(key)
    bad_fit[0] ^= 1
    bad_key[0] ^= 1
    case("malformed_fit_magic", bytes(bad_fit), key, "REJECT")
    case("malformed_key_magic", fit, bytes(bad_key), "REJECT")
    report = {"status": "PASS" if all(c["pass"] for c in cases) else "FAIL",
              "cases": cases, "case_count": len(cases), "source_commit": commit,
              "inputs": {str(x): sha(x.read_bytes()) for x in (args.fit, args.control, args.certificate, args.config, args.contract, args.payload)},
              "tools": {n: sha((args.tools/n).read_bytes()) for n in ("mkimage", "fit_check_sign")},
              "public_rsa_pkcs1_der_sha256": sha(der), "rsa_key_math": "PASS",
              "control_nonanchor_properties_and_reservations": "UNCHANGED", "signed_nodes": paths,
              "limits": ["Host software RSA only; target hardware RSA is UNVERIFIED",
                         "-s exercises Host loadable image list, not actual SPL execution",
                         "default checker ignores corrupted pure-loadable payload",
                         "required-tag/hardware factors require independent contract/math checks",
                         "no board, policy, preload, SMC, MCU, cache or RPMsg execution proof"],
              "private_fixture": "fresh ignored output, mode0600; never publish"}
    (out / "report.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps({"status": report["status"], "case_count": len(cases),
                      "failed_cases": [c["name"] for c in cases if not c["pass"]],
                      "report": str(out / "report.json")}, indent=2))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
