#!/usr/bin/env python3
"""Prepare a fresh Host-only signed Stage B addition. Never access the board.

Keep v6 Linux/DT/root guards; copy the fixed signed v8 FIT without modification.
The generated stdin installer is a passive, single-directory addition. It is
not run here. The parent reviews and owns any subsequent board interaction.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tarfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT / "artifacts/local/p029-packet-v6"
OLD_SCR = ROOT / "artifacts/local/p029-root-fix-v6/source/stage-B.scr"
BASELINE = ROOT / "artifacts/local/p029-signed-default-linux-20261004/baseline.stdout"
HELPER = ROOT / "scripts/board/p029_install_root_fix.py"
HELPER_SHA = "b0b501a5caea060c4f2775f3d7974f3a20c5a08a1166402fa527c374773b3ef2"
SIGNED = ROOT / "artifacts/local/p029-signed-v8/amp-signed.itb"
SIGNED_SHA = "331431fcf1f914c91efd54ffb52a50f9e0ef17ea9df7ba2ed8ef7c675ed6c041"
SIGNED_BYTES = 130560
UBOOT_SHA = "ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4"
OLD_B_SHA = "bdd9284ca70be822be9ec980d907bc4675c0bfe0028e2d88a7384da4ec4a6d8e"
SOURCE = "/dev/shm/amp-p029-signed-stage-b-v8"
DEST = "/boot/amp-p029/signature-fix-v8"
MEMBERS = {"amp-signed.itb", "stage-B.cmd", "stage-B.scr"}
MANIFEST = "SIGNED_STAGE_B.json"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(value, message):
    if not value:
        raise ValueError(message)


def script_text(data):
    require(len(data) >= 72, "short SCRIPT")
    h = struct.unpack(">7I4B32s", data[:64])
    header = bytearray(data[:64]); header[4:8] = bytes(4)
    require(h[0] == 0x27051956 and h[1] == zlib.crc32(header), "SCRIPT header CRC")
    require(h[3] + 64 == len(data) and h[6] == zlib.crc32(data[64:]), "SCRIPT size/data CRC")
    require(h[7:11] == (5, 7, 6, 0), "SCRIPT Linux/PPC/script/none")
    require(struct.unpack(">II", data[64:72]) == (len(data)-72, 0), "single SCRIPT component")
    return data[72:]


# Reuse the reviewed root-fix utility functions verbatim. Its runtime assertions
# remain active because the generated entry unconditionally rejects python -O.
INSTALLER_BODY = r'''
def fail_if(condition, message):
    if condition:
        raise RuntimeError(message)


def verify_uboot():
    expected = 0x4000
    for item in ("start", "size"):
        fail_if(int(Path("/sys/class/block/mmcblk0p1").joinpath(item).read_text()) != expected,
                "STOP unexpected uboot partition " + item)
    fail_if(int(Path("/sys/class/block/mmcblk0/queue/logical_block_size").read_text()) != 512,
            "STOP sector size")
    fields = dict(line.split("=", 1) for line in Path("/sys/class/block/mmcblk0p1/uevent").read_text().splitlines() if "=" in line)
    name = fields.get("PARTNAME")
    fail_if(name is not None and name != "uboot", "STOP contradictory partition name")
    link = Path("/dev/disk/by-partlabel/uboot")
    named_link = os.path.lexists(link)
    fail_if(named_link and link.resolve(strict=True) != Path("/dev/mmcblk0p1"), "STOP partition label target")
    fail_if(name != "uboot" and not named_link, "STOP missing partition name evidence")
    fd = os.open("/dev/mmcblk0p1", os.O_RDONLY | os.O_NOFOLLOW)
    try:
        fail_if(not stat.S_ISBLK(os.fstat(fd).st_mode), "STOP uboot device type")
        h, remaining = hashlib.sha256(), 8388608
        while remaining:
            chunk = os.read(fd, min(1048576, remaining))
            fail_if(not chunk, "STOP short uboot read")
            h.update(chunk); remaining -= len(chunk)
        fail_if(h.hexdigest() != UBOOT_SHA, "STOP new v8 uboot identity")
    finally:
        os.close(fd)


def default_linux():
    fail_if(os.uname().release != "6.1.99-rk3576", "STOP default kernel")
    args = Path("/proc/cmdline").read_text().split()
    fail_if(not any("uboot-f8b4554" in x for x in args) or any(x.startswith("amp_test_stage=") for x in args), "STOP boot chain/stage")
    fail_if("root=/dev/mmcblk0p3" not in args, "STOP root argument")
    fail_if(Path("/sys/firmware/devicetree/base/model").read_bytes().rstrip(b"\0") != b"EmbedFire LubanCat-3-v2", "STOP model")
    for target, source in [("/boot", "/dev/mmcblk0p2"), ("/", "/dev/mmcblk0p3")]:
        fail_if(subprocess.check_output(["findmnt", "-n", "-o", "SOURCE", "--target", target], text=True).strip() != source, "STOP mounted source")
    fail_if(any(Path("/sys/bus/rpmsg/devices").glob("*")), "STOP existing RPMsg device")
    fail_if(any(x.split()[0] == "rk3576_amp_echo_test" for x in Path("/proc/modules").read_text().splitlines()), "STOP echo module loaded")


def baseline_state(spec, allow_added=False):
    old = spec["old_passive_files"]
    fail_if(set(old) != EXPECTED_OLD_NAMES, "STOP old file whitelist")
    top = {name.split("/", 1)[0] for name in old}
    fail_if({p.name for p in BOOT.iterdir()} != top | ({DEST.name} if allow_added else set()), "STOP passive directory members")
    result = {}
    for directory in ("script-fix-v5", "root-fix-v6"):
        protected_directory(BOOT/directory)
        fail_if({p.name for p in (BOOT/directory).iterdir()} != {name.split("/", 1)[1] for name in old if name.startswith(directory+"/")}, "STOP old nested members")
    for name, expected in old.items():
        p = BOOT/name; s = p.lstat()
        fail_if(not stat.S_ISREG(s.st_mode) or s.st_uid != 0 or s.st_nlink != 1 or s.st_mode & 0o022, "STOP old file type/owner: " + name)
        fail_if(s.st_size != expected["size"] or file_sha(p) != expected["sha256"], "STOP old file identity: " + name)
        result[name] = [expected["sha256"], s.st_mode, s.st_size, s.st_uid, s.st_gid, s.st_mtime_ns, s.st_nlink]
    return result


def take_snapshot(trusted):
    fail_if(trusted != PINNED_MANIFEST_SHA, "STOP wrong pinned manifest SHA required")
    fd = os.open(SOURCE, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        fail_if(s.st_uid != int(os.environ["SUDO_UID"]) or stat.S_IMODE(s.st_mode) != 0o700, "STOP source ownership/mode")
        fail_if(set(os.listdir(fd)) != MEMBERS | {MANIFEST}, "STOP RAM source whitelist")
        raw = read_member(fd, MANIFEST, 32768)
        fail_if(digest(raw) != trusted, "STOP untrusted manifest")
        spec = json.loads(raw)
        fail_if(spec["id"] != "P029_SIGNED_STAGE_B_V8" or spec["destination"] != str(DEST) or spec["source"] != str(SOURCE), "STOP manifest identity")
        fail_if(set(spec["files"]) != MEMBERS or spec["uboot_sha256"] != UBOOT_SHA, "STOP manifest target identity")
        data = {name: read_member(fd, name, 1048576) for name in MEMBERS}
        for name, content in data.items():
            fail_if(len(content) != spec["files"][name]["size"] or digest(content) != spec["files"][name]["sha256"], "STOP snapshot identity: " + name)
        fail_if(len(data["amp-signed.itb"]) != 130560 or digest(data["amp-signed.itb"]) != SIGNED_SHA, "STOP signed FIT identity")
        data[MANIFEST] = raw
        return data, spec
    finally:
        os.close(fd)


def rename_noreplace(source, dest):
    import ctypes
    libc = ctypes.CDLL(None, use_errno=True)
    fn = libc.renameat2
    fn.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    fn.restype = ctypes.c_int
    if fn(-100, os.fsencode(source), -100, os.fsencode(dest), 1):
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code))


def sync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def write_new(directory, name, content):
    fd = os.open(directory/name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
    try:
        with os.fdopen(fd, "wb", closefd=False) as f:
            f.write(content); f.flush(); os.fsync(fd); os.fchmod(fd, 0o644)
    finally:
        os.close(fd)


def install(trusted):
    fail_if(not __debug__, "STOP optimized Python forbidden")
    fail_if(os.geteuid() != 0 or os.environ.get("SUDO_UID", "0") == "0", "STOP sudo operator identity")
    default_linux()
    verify_uboot()
    for p in [Path("/boot"), BOOT]:
        protected_directory(p)
    fail_if(os.path.lexists(DEST), "STOP existing signature fix; inspect, never overwrite")
    data, spec = take_snapshot(trusted)
    before, factory_before = baseline_state(spec), factory_state()
    fail_if(os.statvfs(BOOT).f_bavail * os.statvfs(BOOT).f_frsize < 10*1024*1024, "STOP boot free space")
    data["SHA256SUMS"] = "".join(f"{digest(content)}  {name}\n" for name,content in sorted(data.items())).encode()
    staged = Path(tempfile.mkdtemp(prefix=".signature-fix-v8-", dir=BOOT))
    committed = False
    try:
        for name, content in data.items():
            write_new(staged, name, content)
            fail_if(file_sha(staged/name) != digest(content), "STOP staged identity")
        staged.chmod(0o755)
        sync_dir(staged)
        rename_noreplace(staged, DEST); committed = True
        sync_dir(BOOT)
        fail_if(baseline_state(spec, True) != before or factory_state() != factory_before, "STOP original file metadata changed")
        default_linux(); verify_uboot()
        for name, content in data.items():
            fail_if(file_sha(DEST/name) != digest(content), "STOP installed identity")
        receipt = {"result": "SIGNED_STAGE_B_INSTALLED_READBACK_PASS", "destination": str(DEST), "manifest_sha256": trusted,
                   "files": {name: {"size":len(content),"sha256":digest(content)} for name,content in sorted(data.items())},
                   "old_passive_23_and_factory_six_unchanged": True, "default_links_path_metadata_unchanged": True,
                   "uboot_sha256": UBOOT_SHA, "kernel": os.uname().release, "M0_started": False, "reboot": False,
                   "target_hardware_signature": "UNVERIFIED", "D_ready": False}
        raw_receipt = (json.dumps(receipt, indent=2, sort_keys=True)+"\n").encode()
        write_new(DEST, "INSTALL_RECEIPT.json", raw_receipt)
        sync_dir(DEST)
        fail_if({p.name for p in DEST.iterdir()} != set(data) | {"INSTALL_RECEIPT.json"}, "STOP new directory whitelist")
        fail_if(file_sha(DEST/"INSTALL_RECEIPT.json") != digest(raw_receipt), "STOP receipt identity")
        fail_if(baseline_state(spec, True) != before or factory_state() != factory_before, "STOP final original file metadata changed")
    finally:
        if not committed:
            for name in data:
                p = staged/name
                if p.exists():
                    p.unlink()
            staged.rmdir()
    receipt["receipt_sha256"] = digest(raw_receipt)
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] != "--approved-signed-stage-b-v8" or sys.argv[2] != PINNED_MANIFEST_SHA:
        raise SystemExit("STOP explicit task approval and pinned manifest SHA required")
    install(sys.argv[2])
'''

CLEANUP = r'''
#!/usr/bin/env python3
"""Remove only the verified task RAM source. Never touch persistent files."""
import hashlib,json,os,stat
from pathlib import Path
SOURCE = Path(SOURCE_VALUE)
TRUSTED = TRUSTED_VALUE
fd = os.open(SOURCE, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
try:
    s = os.fstat(fd)
    if s.st_uid != os.getuid() or stat.S_IMODE(s.st_mode) != 0o700:
        raise SystemExit("STOP RAM ownership")
    names = {"SIGNED_STAGE_B.json", "amp-signed.itb", "stage-B.cmd", "stage-B.scr"}
    if set(os.listdir(fd)) != names:
        raise SystemExit("STOP RAM whitelist")
    data = {}
    for name in names:
        item = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
        try:
            fs = os.fstat(item)
            if not stat.S_ISREG(fs.st_mode) or fs.st_uid != os.getuid() or fs.st_nlink != 1 or not 0 < fs.st_size <= 1048576:
                raise SystemExit("STOP RAM member type/size")
            with os.fdopen(item, "rb", closefd=False) as f:
                data[name] = f.read(1048577)
            if len(data[name]) != fs.st_size:
                raise SystemExit("STOP RAM member race")
        finally:
            os.close(item)
    if hashlib.sha256(data["SIGNED_STAGE_B.json"]).hexdigest() != TRUSTED:
        raise SystemExit("STOP RAM manifest")
    spec = json.loads(data["SIGNED_STAGE_B.json"])
    for name,expected in spec["files"].items():
        if len(data[name]) != expected["size"] or hashlib.sha256(data[name]).hexdigest() != expected["sha256"]:
            raise SystemExit("STOP RAM member identity")
    # No deletion before every member is verified; only this exact whitelist.
    for name in names:
        os.unlink(name, dir_fd=fd)
finally:
    os.close(fd)
SOURCE.rmdir()
print("EXACT_SIGNED_B_RAM_SOURCE_REMOVED")
'''


def make(output):
    output = output.resolve()
    require(output.is_relative_to(ROOT/"artifacts/local") and not output.exists(), "fresh ignored Host output required")
    require(subprocess.run(["git", "-C", ROOT, "check-ignore", "-q", output]).returncode == 0, "output must be ignored")
    require(digest(HELPER.read_bytes()) == HELPER_SHA, "reviewed helper changed")
    require(len(SIGNED.read_bytes()) == SIGNED_BYTES and digest(SIGNED.read_bytes()) == SIGNED_SHA, "signed v8 identity")
    old_raw, old_cmd = OLD_SCR.read_bytes(), (OLD/"stage-B.cmd").read_bytes()
    require(digest(old_raw) == OLD_B_SHA and script_text(old_raw) == old_cmd, "reviewed v6 B mismatch")
    baseline_raw = BASELINE.read_bytes(); baseline = json.loads(baseline_raw)
    require(baseline["result"] == "SIGNED_V8_DEFAULT_LINUX_BASELINE_PASS" and baseline["uboot_sha256"] == UBOOT_SHA and baseline["uboot_bytes"] == 8388608, "current v8 baseline")
    require(baseline["kernel"] == "6.1.99-rk3576" and baseline["factory_six"] == "SHA256_MATCH" and baseline["new_signed_path"] == "ABSENT" and len(baseline["old_passive_files"]) == 23, "old material snapshot")
    for name in ("Image", "initrd", "stage-B.dtb"):
        expected = baseline["old_passive_files"][name]
        require((OLD/"boot"/name).stat().st_size == expected["size"] and digest((OLD/"boot"/name).read_bytes()) == expected["sha256"], "paired asset changed")
    text = old_cmd.decode()
    old_call = "if amp_m0load /amp-p029/amp-host.itb 0x48300000; then"
    require(text.count(old_call) == 1, "exactly one original M0 invocation")
    path = "/amp-p029/signature-fix-v8/amp-signed.itb"
    guard = ("if setenv filesize; then true; else echo STOP clear signed FIT size; exit 1; fi\n"
             f"if size mmc 0:${{p029_part}} {path}; then true; else echo STOP signed FIT size; exit 1; fi\n"
             f'if test "${{filesize}}" = "0x{SIGNED_BYTES:x}"; then true; else echo STOP signed FIT length; exit 1; fi\n')
    text = text.replace("echo P029 stage B explicit test - factory default unchanged", "echo P029 SignedFix-v8 stage B explicit test - factory default unchanged", 1)
    text = text.replace(old_call, guard+f"if amp_m0load {path} 0x48300000; then", 1)
    require(text.count("amp_m0load ") == 1 and "amp_test_stage=B" in text and "insmod" not in text, "single B-only scope")
    output.mkdir(mode=0o700); source = output/"source"; source.mkdir(mode=0o700)
    (source/"stage-B.cmd").write_text(text)
    (source/"amp-signed.itb").write_bytes(SIGNED.read_bytes())
    env = dict(os.environ, SOURCE_DATE_EPOCH="1790985600")
    run = subprocess.run(["mkimage", "-A", "ppc", "-O", "linux", "-T", "script", "-C", "none", "-n", "P029 SignedFix-v8 Stage B", "-d", source/"stage-B.cmd", source/"stage-B.scr"], capture_output=True, env=env)
    (output/"mkimage.stdout").write_bytes(run.stdout); (output/"mkimage.stderr").write_bytes(run.stderr)
    require(run.returncode == 0, "SCRIPT generation failed")
    require(script_text((source/"stage-B.scr").read_bytes()) == text.encode(), "actual SCRIPT CRC/text mismatch")
    spec = {"id":"P029_SIGNED_STAGE_B_V8", "source":SOURCE, "destination":DEST, "uboot_sha256":UBOOT_SHA,
            "source_control_dt_sha256":"43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce",
            "baseline_source_sha256":digest(baseline_raw), "old_passive_files":baseline["old_passive_files"],
            "files":{name:{"size":(source/name).stat().st_size,"sha256":digest((source/name).read_bytes())} for name in sorted(MEMBERS)},
            "scope":"passive new B-only directory; retains factory, old23, modules, GPT and all existing files",
            "stage_B_kernel":"6.1.99-rk3576-m0echo-p026", "stage_script_only_after_parent_review":True,
            "target_hardware_signature":"UNVERIFIED", "M0_execution":"UNVERIFIED", "D_ready":False}
    raw = (json.dumps(spec,indent=2,sort_keys=True)+"\n").encode(); (source/MANIFEST).write_bytes(raw)
    trusted = digest(raw)
    parsed = ast.parse(HELPER.read_text())
    selected = []
    for node in parsed.body:
        if isinstance(node, ast.Assign) and any(isinstance(n, ast.Name) and n.id in ("FACTORY", "LINKS") for n in node.targets):
            selected.append(ast.get_source_segment(HELPER.read_text(), node))
        elif isinstance(node, ast.FunctionDef) and node.name in ("digest", "file_sha", "protected_directory", "read_member", "factory_state"):
            selected.append(ast.get_source_segment(HELPER.read_text(), node))
    require(len(selected) == 7, "reviewed helper extraction")
    prologue = ("#!/usr/bin/env python3\n\"\"\"Parent-reviewed passive signed B addition; no M0/reboot/KO.\"\"\"\n"
                "import hashlib,json,os,stat,subprocess,sys,tempfile\nfrom pathlib import Path\n"
                f"SOURCE=Path({SOURCE!r})\nBOOT=Path('/boot/amp-p029')\nDEST=Path({DEST!r})\n"
                f"UBOOT_SHA={UBOOT_SHA!r}\nSIGNED_SHA={SIGNED_SHA!r}\nPINNED_MANIFEST_SHA={trusted!r}\n"
                f"MANIFEST={MANIFEST!r}\nMEMBERS={sorted(MEMBERS)!r}\nMEMBERS=set(MEMBERS)\n"
                f"EXPECTED_OLD_NAMES=set({sorted(spec['old_passive_files'])!r})\n")
    (output/"install-signed-stage-b.py").write_text(prologue+"\n\n".join(selected)+"\n"+INSTALLER_BODY)
    (output/"cleanup-ram.py").write_text(CLEANUP.replace("SOURCE_VALUE", repr(SOURCE)).replace("TRUSTED_VALUE", repr(trusted)).lstrip())
    with tarfile.open(output/"source.tar", "w", format=tarfile.USTAR_FORMAT) as archive:
        for name in sorted(MEMBERS | {MANIFEST}):
            p = source/name; info = archive.gettarinfo(p, arcname=name)
            info.mode, info.uid, info.gid, info.uname, info.gname, info.mtime = 0o644, 0, 0, "", "", 1790985600
            with p.open("rb") as f:
                archive.addfile(info, f)
    result = {"result":"HOST_GENERATED_PENDING_PARENT_REVIEW", "board_access":False, "installer_executed":False,
              "manifest_sha256":trusted, "files":spec["files"], "source_tar_sha256":digest((output/"source.tar").read_bytes()),
              "source_tar_bytes":(output/"source.tar").stat().st_size,
              "installer_sha256":digest((output/"install-signed-stage-b.py").read_bytes()),
              "cleanup_sha256":digest((output/"cleanup-ram.py").read_bytes()),
              "mkimage_exit":run.returncode, "original_v6_scr_sha256":OLD_B_SHA, "baseline_source_sha256":digest(baseline_raw),
              "reused_root_fix_helper_sha256":HELPER_SHA, "new_persistent_files":sorted(MEMBERS|{MANIFEST,"SHA256SUMS","INSTALL_RECEIPT.json"}),
              "B_or_hardware_signature_tested":False, "D_ready":False}
    (output/"result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    return result


if __name__ == "__main__":
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--output", type=Path, required=True)
    make(args.parse_args().output)
