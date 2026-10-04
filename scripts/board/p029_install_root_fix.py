#!/usr/bin/env python3
"""Approved P029 root-argument repair: add a new script directory; retain installed v4 files.

Stream this reviewed Host source to sudo python3 stdin with the Host-pinned
SCRIPT_FIX.json SHA. No reboot, module, M0, MMIO or bootloader operation.
"""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

SOURCE = Path("/dev/shm/amp-p029-root-fix-v6")
BOOT = Path("/boot/amp-p029")
DEST = BOOT / "root-fix-v6"
SCRIPTS = {f"stage-{stage}.scr" for stage in "ABC"}
BASE_NAMES = SCRIPTS | {"Image", "initrd", "stage-A.dtb", "stage-B.dtb", "stage-C.dtb", "amp-host.itb", "rk3576_amp_echo_test.ko"}
RECEIPTS = {".p029-MANIFEST.json", ".p029-boot.sha256", ".p029-modules.sha256"}
FACTORY = {
    "Image": "e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e",
    "initrd": "425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397",
    "rk-kernel.dtb": "76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90",
    "boot.scr": "c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325",
    "boot.cmd": "9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55",
    "uEnv/uEnv.txt": "4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64",
}
LINKS = {"Image": "Image-6.1.99-rk3576", "initrd": "initrd.img-6.1.99-rk3576", "rk-kernel.dtb": "dtb/rk3576-lubancat-3-v2.dtb"}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1048576), b""):
            h.update(chunk)
    return h.hexdigest()


def protected_directory(path):
    s = path.lstat()
    assert stat.S_ISDIR(s.st_mode) and s.st_uid == 0 and not s.st_mode & 0o022, path


def read_member(directory_fd, name, limit):
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory_fd)
    try:
        s = os.fstat(fd)
        assert stat.S_ISREG(s.st_mode) and s.st_nlink == 1 and 0 < s.st_size <= limit, name
        with os.fdopen(fd, "rb", closefd=False) as f:
            data = f.read(limit+1)
        assert len(data) == s.st_size, name
        return data
    finally:
        os.close(fd)


def factory_state():
    result = {}
    for name, expected in FACTORY.items():
        p = Path("/boot")/name; s = p.lstat()
        assert s.st_uid == 0 and file_sha(p) == expected, name
        if name in LINKS:
            assert stat.S_ISLNK(s.st_mode) and os.readlink(p) == LINKS[name], name
        else:
            assert stat.S_ISREG(s.st_mode), name
        result[name] = [expected, s.st_mode, s.st_size, s.st_uid, s.st_gid, s.st_mtime_ns, LINKS.get(name)]
    return result


def verify_baseline(spec):
    assert set(spec["baseline_boot"]) == BASE_NAMES
    assert set(spec["baseline_receipts"]) == RECEIPTS
    previous = BOOT / "script-fix-v5"
    protected_directory(previous)
    assert set(spec["previous_fix"]) == SCRIPTS | {"SCRIPT_FIX.json", "SHA256SUMS"}
    assert {p.name for p in previous.iterdir()} == set(spec["previous_fix"])
    for name, expected in spec["previous_fix"].items():
        p = previous/name; s = p.lstat()
        assert stat.S_ISREG(s.st_mode) and s.st_uid == 0 and s.st_nlink == 1 and not s.st_mode & 0o022, name
        assert file_sha(p) == expected, name
    for name, expected in {**spec["baseline_boot"], **spec["baseline_receipts"]}.items():
        p = BOOT/name; s = p.lstat()
        assert stat.S_ISREG(s.st_mode) and s.st_uid == 0 and s.st_nlink == 1 and not s.st_mode & 0o022, name
        assert file_sha(p) == expected, name


def install(trusted):
    assert os.geteuid() == 0 and os.environ.get("SUDO_UID", "0") != "0"
    assert os.uname().release == "6.1.99-rk3576"
    assert "uboot-f8b4554" in Path("/proc/cmdline").read_text()
    assert Path("/sys/firmware/devicetree/base/model").read_bytes().rstrip(b"\0") == b"EmbedFire LubanCat-3-v2"
    for target, source in [("/boot", "/dev/mmcblk0p2"), ("/", "/dev/mmcblk0p3")]:
        assert subprocess.check_output(["findmnt", "-n", "-o", "SOURCE", "--target", target], text=True).strip() == source
    for path in [Path("/boot"), BOOT]:
        protected_directory(path)
    assert not os.path.lexists(DEST), "STOP existing script fix; inspect instead of overwrite"
    assert {p.name for p in BOOT.iterdir()} == BASE_NAMES | RECEIPTS | {"script-fix-v5"}
    before = factory_state()
    fd = os.open(SOURCE, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        assert s.st_uid == int(os.environ["SUDO_UID"]) and stat.S_IMODE(s.st_mode) == 0o700
        assert set(os.listdir(fd)) == SCRIPTS | {"SCRIPT_FIX.json"}
        manifest = read_member(fd, "SCRIPT_FIX.json", 16384)
        assert digest(manifest) == trusted, "STOP untrusted repair manifest"
        spec = json.loads(manifest)
        assert spec["id"] == "P029_ROOT_FIX_V6" and spec["destination"] == str(DEST)
        assert spec["baseline_packet_sha256"] == "3b7ae414d4a23474ce487d438c952bc6c1e796bb38f2611d599f7206dfbeb7c4"
        assert set(spec["scripts"]) == SCRIPTS
        # Verified immutable in-memory snapshot precedes all persistent writes.
        data = {name: read_member(fd, name, 65536) for name in SCRIPTS}
        for name, content in data.items():
            assert len(content) == spec["scripts"][name]["size"] and digest(content) == spec["scripts"][name]["sha256"], name
    finally:
        os.close(fd)
    verify_baseline(spec)
    assert os.statvfs(BOOT).f_bavail * os.statvfs(BOOT).f_frsize >= 10*1024*1024
    staged = Path(tempfile.mkdtemp(prefix=".root-fix-v6-", dir=BOOT))
    data["SCRIPT_FIX.json"] = manifest
    data["SHA256SUMS"] = "".join(f"{digest(value)}  {name}\n" for name,value in sorted(data.items())).encode()
    committed = False
    try:
        for name, content in data.items():
            with (staged/name).open("xb") as f:
                f.write(content); f.flush(); os.fsync(f.fileno()); os.fchmod(f.fileno(), 0o644)
            assert file_sha(staged/name) == digest(content)
        staged.chmod(0o755)
        assert not os.path.lexists(DEST)
        staged.rename(DEST); committed = True
        subprocess.run(["sync", "-f", str(BOOT)], check=True)
        verify_baseline(spec)
        assert factory_state() == before
        for name, content in data.items():
            assert file_sha(DEST/name) == digest(content), name
    finally:
        if not committed:
            for name in data:
                p = staged/name
                if p.exists(): p.unlink()
            staged.rmdir()
    print(json.dumps({"result": "SCRIPT_FIX_INSTALLED_READBACK_PASS", "destination": str(DEST),
                      "scripts": spec["scripts"], "manifest_sha256": trusted,
                      "baseline_boot_and_receipts_unchanged": True, "factory_contents_links_path_metadata_unchanged": True,
                      "M0_started": False, "reboot": False, "kernel": os.uname().release}, sort_keys=True))


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] != "--approved-root-fix-v6" or len(sys.argv[2]) != 64 or any(c not in "0123456789abcdef" for c in sys.argv[2]):
        raise SystemExit("STOP explicit task approval and pinned manifest SHA required")
    install(sys.argv[2])
