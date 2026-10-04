#!/usr/bin/env python3
"""Install the P030 diagnostic B files as a passive, additive boot partition directory.

Run only after reviewing this source and P030_INITDIAG.json. The script requires
the pinned manifest and installer hashes as explicit arguments, rechecks the
current default boot, U-Boot identity, factory files, and existing P029 tree,
then atomically adds one new directory. It never starts M0 or reboots.
"""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

SOURCE = Path("/dev/shm/amp-p030-initdiag-v1")
BOOT = Path("/boot/amp-p029")
DEST = BOOT / "initdiag-v1"
MANIFEST = "P030_INITDIAG.json"
SUMS = "SHA256SUMS"
INSTALLER = "install-p030.py"
PAYLOADS = {"amp-signed.itb", "stage-B.cmd", "stage-B.scr"}
PINNED_MANIFEST_SHA = "81d4c62833ac1ce29c6172b2281e338ef2bdec9310cfc75d1edac62b4146d04f"


def fail_if(condition, message):
    if condition:
        raise RuntimeError("STOP " + message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        fail_if(not stat.S_ISREG(before.st_mode) or before.st_nlink != 1, "file type/link count: " + str(path))
        h = hashlib.sha256()
        while True:
            part = os.read(fd, 1024 * 1024)
            if not part:
                break
            h.update(part)
        after = os.fstat(fd)
        fail_if((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns), "file changed while reading: " + str(path))
        return h.hexdigest()
    finally:
        os.close(fd)


def protected_directory(path):
    s = path.lstat()
    fail_if(not stat.S_ISDIR(s.st_mode) or s.st_uid != 0 or s.st_gid != 0 or s.st_mode & 0o022,
            "directory type/owner/mode: " + str(path))
    return s


def read_member(directory_fd, name, limit, owner):
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory_fd)
    try:
        s = os.fstat(fd)
        fail_if(not stat.S_ISREG(s.st_mode) or s.st_nlink != 1 or s.st_uid != owner or
                stat.S_IMODE(s.st_mode) != 0o644 or not 0 < s.st_size <= limit,
                "RAM source type/owner/mode/size: " + name)
        with os.fdopen(fd, "rb", closefd=False) as f:
            data = f.read(limit + 1)
        fail_if(len(data) != s.st_size, "RAM source changed while reading: " + name)
        return data
    finally:
        os.close(fd)


def source_snapshot(trusted_manifest_sha, trusted_installer_sha):
    owner = int(os.environ.get("SUDO_UID", "0"))
    fail_if(owner == 0, "missing non-root sudo operator identity")
    fd = os.open(SOURCE, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        fail_if(s.st_uid != owner or s.st_gid != owner or stat.S_IMODE(s.st_mode) != 0o700,
                "RAM source directory owner/mode")
        expected = PAYLOADS | {MANIFEST, SUMS, INSTALLER}
        fail_if(set(os.listdir(fd)) != expected, "RAM source whitelist")
        data = {name: read_member(fd, name, 1024 * 1024, owner) for name in sorted(expected)}
        fail_if(digest(data[MANIFEST]) != trusted_manifest_sha or trusted_manifest_sha != PINNED_MANIFEST_SHA,
                "pinned manifest SHA")
        fail_if(digest(data[INSTALLER]) != trusted_installer_sha, "pinned installer SHA")
        spec = json.loads(data[MANIFEST])
        fail_if(spec.get("id") != "P030_M0_INITDIAG_V1" or spec.get("destination") != str(DEST),
                "manifest identity/destination")
        fail_if(set(spec.get("files", {})) != PAYLOADS, "manifest payload whitelist")
        for name in PAYLOADS:
            item = spec["files"][name]
            fail_if(len(data[name]) != item.get("size") or digest(data[name]) != item.get("sha256"),
                    "manifest payload identity: " + name)
        checksummed = {name: data[name] for name in PAYLOADS | {MANIFEST}}
        expected_sums = "".join(f"{digest(content)}  {name}\n" for name, content in sorted(checksummed.items())).encode()
        fail_if(data[SUMS] != expected_sums, "package SHA256SUMS")
        fail_if(spec.get("stage_b", {}).get("m0_load_invocations") != 1 or
                spec["stage_b"].get("loads_ko") is not False or
                spec["stage_b"].get("includes_stage_c") is not False,
                "stage-B scope")
        return data, spec, owner
    finally:
        os.close(fd)


def default_linux(spec):
    expected = spec["board_precondition"]
    fail_if(os.uname().release != expected["kernel_release"], "kernel release")
    fail_if(Path("/sys/firmware/devicetree/base/model").read_bytes().rstrip(b"\0").decode() != expected["model"],
            "board model")
    args = Path("/proc/cmdline").read_text().split()
    fail_if(any(token not in args and token not in " ".join(args) for token in expected["cmdline_contains"]),
            "default command line")
    fail_if(any(arg.startswith(prefix) for arg in args for prefix in expected["cmdline_absent_prefixes"]),
            "test-stage command line")
    fail_if("root=" + expected["root_mount"] not in args, "root argument")
    for target, source in [("/boot", expected["boot_mount"]), ("/", expected["root_mount"])]:
        actual = subprocess.check_output(["findmnt", "-n", "-o", "SOURCE", "--target", target], text=True).strip()
        fail_if(actual != source, "mounted source: " + target)
    fail_if(any(Path("/sys/bus/rpmsg/devices").glob("*")), "RPMsg device already present")
    fail_if(any(line.split() and line.split()[0] == "rk3576_amp_echo_test"
                for line in Path("/proc/modules").read_text().splitlines()), "echo module already loaded")


def verify_uboot(spec):
    expected = spec["board_precondition"]["uboot_partition"]
    for field, sysfs_name in (("start_sectors", "start"), ("size_sectors", "size")):
        fail_if(int(Path("/sys/class/block/mmcblk0p1", sysfs_name).read_text()) != expected[field],
                "U-Boot partition " + sysfs_name)
    fail_if(int(Path("/sys/class/block/mmcblk0/queue/logical_block_size").read_text()) !=
            expected["logical_block_size"], "eMMC sector size")
    link = Path("/dev/disk/by-partlabel/uboot")
    fail_if(os.path.lexists(link) and link.resolve(strict=True) != Path(expected["path"]), "U-Boot label target")
    fd = os.open(expected["path"], os.O_RDONLY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        fail_if(not stat.S_ISBLK(s.st_mode), "U-Boot device type")
        remaining, h = expected["bytes"], hashlib.sha256()
        while remaining:
            block = os.read(fd, min(1024 * 1024, remaining))
            fail_if(not block, "short U-Boot partition read")
            h.update(block)
            remaining -= len(block)
        fail_if(h.hexdigest() != expected["sha256"], "U-Boot partition SHA")
        return h.hexdigest()
    finally:
        os.close(fd)


def factory_state(spec):
    result = {}
    for relative, expected in spec["board_precondition"]["factory_boot_files"].items():
        path = Path("/boot") / relative
        s = path.lstat()
        if expected["kind"] == "symlink":
            fail_if(not stat.S_ISLNK(s.st_mode) or os.readlink(path) != expected["target"],
                    "factory symlink: " + relative)
            content_path = path.resolve(strict=True)
            fail_if(not stat.S_ISREG(content_path.lstat().st_mode), "factory symlink target type: " + relative)
        else:
            fail_if(not stat.S_ISREG(s.st_mode), "factory file type: " + relative)
            content_path = path
        fail_if(s.st_uid != 0 or s.st_gid != 0 or file_sha(content_path) != expected["sha256"],
                "factory file identity: " + relative)
        result[relative] = [expected["sha256"], s.st_mode, s.st_size, s.st_uid, s.st_gid,
                            s.st_mtime_ns, os.readlink(path) if stat.S_ISLNK(s.st_mode) else None]
    return result


def snapshot_tree(root, exclude_top=()):
    rows = []

    def walk(directory, prefix=""):
        for entry in sorted(os.scandir(directory), key=lambda item: item.name):
            if not prefix and entry.name in exclude_top:
                continue
            path = Path(entry.path)
            rel = (prefix + "/" + entry.name).lstrip("/")
            s = entry.stat(follow_symlinks=False)
            common = [rel, s.st_mode, s.st_uid, s.st_gid, s.st_size, s.st_mtime_ns, s.st_nlink]
            fail_if(s.st_uid != 0 or s.st_gid != 0 or s.st_mode & 0o022, "existing tree owner/mode: " + rel)
            if stat.S_ISDIR(s.st_mode):
                rows.append(common + ["dir", None])
                walk(path, rel)
            elif stat.S_ISREG(s.st_mode):
                fail_if(s.st_nlink != 1, "existing file link count: " + rel)
                rows.append(common + ["file", file_sha(path)])
            else:
                fail_if(True, "unexpected existing tree object: " + rel)

    walk(root)
    return rows


def verify_preexisting_tree(spec, allow_dest=False):
    expected = spec["board_precondition"]
    protected_directory(Path("/boot"))
    protected_directory(BOOT)
    top = {entry.name for entry in BOOT.iterdir()}
    expected_top = set(expected["existing_amp_p029_top_level"])
    if allow_dest:
        expected_top.add(DEST.name)
    fail_if(top != expected_top, "existing /boot/amp-p029 top-level whitelist")
    for relative, item in expected["existing_amp_p029_files"].items():
        path = BOOT / relative
        s = path.lstat()
        fail_if(not stat.S_ISREG(s.st_mode) or s.st_uid != 0 or s.st_gid != 0 or s.st_nlink != 1 or
                stat.S_IMODE(s.st_mode) != 0o644 or s.st_size != item["size"] or file_sha(path) != item["sha256"],
                "existing P029 file identity: " + relative)
    for relative, item in expected["existing_signature_fix_v8_known_files"].items():
        path = BOOT / "signature-fix-v8" / relative
        s = path.lstat()
        fail_if(not stat.S_ISREG(s.st_mode) or s.st_uid != 0 or s.st_gid != 0 or s.st_nlink != 1 or
                stat.S_IMODE(s.st_mode) != 0o644 or s.st_size != item["size"] or file_sha(path) != item["sha256"],
                "existing v8 file identity: " + relative)
    receipt = expected["existing_signature_fix_v8_dynamic_receipt"]
    rpath = BOOT / "signature-fix-v8" / receipt["name"]
    rs = rpath.lstat()
    fail_if(not stat.S_ISREG(rs.st_mode) or rs.st_uid != 0 or rs.st_gid != 0 or rs.st_nlink != 1 or
            stat.S_IMODE(rs.st_mode) != 0o644 or rs.st_size != receipt["bytes"], "existing v8 receipt identity")
    for directory, names in expected["existing_nested_members"].items():
        path = BOOT / directory
        protected_directory(path)
        fail_if({entry.name for entry in path.iterdir()} != set(names), "existing nested whitelist: " + directory)
    paired = expected["paired_assets"]
    for name, item in paired.items():
        path = BOOT / name
        fail_if(path.stat().st_size != item["size"] or file_sha(path) != item["sha256"],
                "paired Linux asset: " + name)


def sync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def write_new(directory, name, content):
    fd = os.open(directory / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
    try:
        with os.fdopen(fd, "wb", closefd=False) as f:
            f.write(content)
            f.flush()
            os.fsync(fd)
            os.fchmod(fd, 0o644)
    finally:
        os.close(fd)


def rename_noreplace(source, destination):
    import ctypes
    libc = ctypes.CDLL(None, use_errno=True)
    renameat2 = libc.renameat2
    renameat2.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    renameat2.restype = ctypes.c_int
    if renameat2(-100, os.fsencode(source), -100, os.fsencode(destination), 1):
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error))


def cleanup_source(original_data):
    fd = os.open(SOURCE, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        names = set(original_data)
        fail_if(set(os.listdir(fd)) != names, "RAM cleanup whitelist")
        for name, expected in original_data.items():
            actual = read_member(fd, name, 1024 * 1024, int(os.environ["SUDO_UID"]))
            fail_if(actual != expected, "RAM source changed before cleanup: " + name)
        for name in names:
            os.unlink(name, dir_fd=fd)
    finally:
        os.close(fd)
    SOURCE.rmdir()


def install(trusted_manifest_sha, trusted_installer_sha):
    fail_if(not __debug__, "Python -O is forbidden")
    fail_if(os.geteuid() != 0 or os.environ.get("SUDO_UID", "0") == "0", "sudo operator identity")
    fail_if(len(trusted_installer_sha) != 64 or any(c not in "0123456789abcdef" for c in trusted_installer_sha),
            "installer SHA syntax")
    data, spec, owner = source_snapshot(trusted_manifest_sha, trusted_installer_sha)
    default_linux(spec)
    uboot_sha = verify_uboot(spec)
    factory_before = factory_state(spec)
    verify_preexisting_tree(spec)
    fail_if(os.path.lexists(DEST), "destination already exists; inspect rather than overwrite")
    space = os.statvfs(BOOT)
    fail_if(space.f_bavail * space.f_frsize < 1024 * 1024, "insufficient boot partition space")
    tree_before = snapshot_tree(BOOT)
    before_top = {entry.name for entry in BOOT.iterdir()}
    to_install = {name: data[name] for name in PAYLOADS | {MANIFEST, SUMS}}
    staged = Path(tempfile.mkdtemp(prefix=".p030-initdiag-v1-", dir=BOOT))
    committed = False
    try:
        for name, content in to_install.items():
            write_new(staged, name, content)
            fail_if(file_sha(staged / name) != digest(content), "staged file readback: " + name)
        staged.chmod(0o755)
        sync_dir(staged)
        rename_noreplace(staged, DEST)
        committed = True
        sync_dir(BOOT)
        default_linux(spec)
        fail_if(verify_uboot(spec) != uboot_sha, "U-Boot changed during install")
        fail_if(factory_state(spec) != factory_before, "factory boot files changed during install")
        verify_preexisting_tree(spec, allow_dest=True)
        fail_if(snapshot_tree(BOOT, {DEST.name}) != tree_before, "existing P029 tree changed during install")
        fail_if({entry.name for entry in BOOT.iterdir()} != before_top | {DEST.name}, "unexpected new P029 entry")
        fail_if({entry.name for entry in DEST.iterdir()} != PAYLOADS | {MANIFEST, SUMS}, "new directory whitelist")
        for name, content in to_install.items():
            fail_if(file_sha(DEST / name) != digest(content), "installed file readback: " + name)
        receipt = {
            "result": "P030_INITDIAG_PASSIVE_STAGE_INSTALLED_READBACK_PASS",
            "destination": str(DEST),
            "manifest_sha256": trusted_manifest_sha,
            "installer_sha256": trusted_installer_sha,
            "installed_files": {name: {"size": len(content), "sha256": digest(content)}
                                for name, content in sorted(to_install.items())},
            "existing_amp_p029_tree_fingerprint": digest(json.dumps(tree_before, separators=(",", ":")).encode()),
            "existing_amp_p029_files_and_metadata_unchanged": True,
            "factory_six_contents_and_links_unchanged": True,
            "uboot_sha256": uboot_sha,
            "kernel": os.uname().release,
            "M0_started": False,
            "KO_loaded": False,
            "reboot": False,
            "COM5_cold_boot_log": "PENDING",
            "target_hardware_signature": "UNVERIFIED",
            "D_ready": False,
        }
        raw_receipt = (json.dumps(receipt, sort_keys=True, indent=2) + "\n").encode()
        write_new(DEST, "INSTALL_RECEIPT.json", raw_receipt)
        sync_dir(DEST)
        fail_if({entry.name for entry in DEST.iterdir()} != PAYLOADS | {MANIFEST, SUMS, "INSTALL_RECEIPT.json"},
                "final new directory whitelist")
        fail_if(file_sha(DEST / "INSTALL_RECEIPT.json") != digest(raw_receipt), "receipt readback")
        default_linux(spec)
        fail_if(verify_uboot(spec) != uboot_sha, "final U-Boot identity")
        fail_if(factory_state(spec) != factory_before, "final factory boot files")
        verify_preexisting_tree(spec, allow_dest=True)
        fail_if(snapshot_tree(BOOT, {DEST.name}) != tree_before, "final existing P029 tree changed")
        fail_if({entry.name for entry in BOOT.iterdir()} != before_top | {DEST.name}, "final P029 top-level whitelist")
        cleanup_source(data)
        receipt["receipt_sha256"] = digest(raw_receipt)
        receipt["ram_source_removed"] = True
        print(json.dumps(receipt, sort_keys=True))
    finally:
        if not committed:
            for name in list(staged.iterdir()):
                name.unlink()
            staged.rmdir()


def resume_receipt(trusted_manifest_sha, trusted_installer_sha):
    fail_if(not __debug__, "Python -O is forbidden")
    fail_if(os.geteuid() != 0 or os.environ.get("SUDO_UID", "0") == "0", "sudo operator identity")
    fail_if(len(trusted_installer_sha) != 64 or any(c not in "0123456789abcdef" for c in trusted_installer_sha),
            "installer SHA syntax")
    data, spec, owner = source_snapshot(trusted_manifest_sha, trusted_installer_sha)
    default_linux(spec)
    uboot_sha = verify_uboot(spec)
    factory_before = factory_state(spec)
    verify_preexisting_tree(spec, allow_dest=True)
    protected_directory(DEST)
    fail_if(not os.path.lexists(DEST / "amp-signed.itb"), "P030 destination is absent")
    fail_if({entry.name for entry in DEST.iterdir()} != PAYLOADS | {MANIFEST, SUMS},
            "partial-install recovery whitelist")
    for name in PAYLOADS | {MANIFEST, SUMS}:
        path = DEST / name
        s = path.lstat()
        fail_if(not stat.S_ISREG(s.st_mode) or s.st_uid != 0 or s.st_gid != 0 or s.st_nlink != 1 or
                stat.S_IMODE(s.st_mode) != 0o644 or file_sha(path) != digest(data[name]),
                "partial-install readback: " + name)
    fail_if(os.path.lexists(DEST / "INSTALL_RECEIPT.json"), "receipt already exists; do not replace")
    tree_before = snapshot_tree(BOOT, {DEST.name})
    before_top = {entry.name for entry in BOOT.iterdir()}
    fail_if(DEST.name not in before_top, "destination missing from parent")
    receipt = {
        "result": "P030_INITDIAG_PASSIVE_STAGE_RECOVERED_READBACK_PASS",
        "destination": str(DEST),
        "manifest_sha256": trusted_manifest_sha,
        "installer_sha256": trusted_installer_sha,
        "recovery_reason": "First install atomically committed the directory, then its post-commit parent whitelist omitted the new directory. All five installed files were independently read back before this receipt-only recovery; no payload was overwritten.",
        "installed_files": {name: {"size": len(data[name]), "sha256": digest(data[name])}
                            for name in sorted(PAYLOADS | {MANIFEST, SUMS})},
        "existing_amp_p029_tree_fingerprint": digest(json.dumps(tree_before, separators=(",", ":")).encode()),
        "existing_amp_p029_files_and_metadata_unchanged": True,
        "factory_six_contents_and_links_unchanged": True,
        "uboot_sha256": uboot_sha,
        "kernel": os.uname().release,
        "M0_started": False,
        "KO_loaded": False,
        "reboot": False,
        "COM5_cold_boot_log": "PENDING",
        "target_hardware_signature": "UNVERIFIED",
        "D_ready": False,
    }
    raw_receipt = (json.dumps(receipt, sort_keys=True, indent=2) + "\n").encode()
    default_linux(spec)
    fail_if(verify_uboot(spec) != uboot_sha, "U-Boot changed before receipt")
    fail_if(factory_state(spec) != factory_before, "factory boot files changed before receipt")
    verify_preexisting_tree(spec, allow_dest=True)
    fail_if(snapshot_tree(BOOT, {DEST.name}) != tree_before, "existing P029 tree changed before receipt")
    write_new(DEST, "INSTALL_RECEIPT.json", raw_receipt)
    sync_dir(DEST)
    fail_if({entry.name for entry in DEST.iterdir()} != PAYLOADS | {MANIFEST, SUMS, "INSTALL_RECEIPT.json"},
            "recovered destination whitelist")
    fail_if(file_sha(DEST / "INSTALL_RECEIPT.json") != digest(raw_receipt), "recovered receipt readback")
    default_linux(spec)
    fail_if(verify_uboot(spec) != uboot_sha, "final U-Boot identity")
    fail_if(factory_state(spec) != factory_before, "final factory boot files")
    verify_preexisting_tree(spec, allow_dest=True)
    fail_if(snapshot_tree(BOOT, {DEST.name}) != tree_before, "final existing P029 tree changed")
    fail_if({entry.name for entry in BOOT.iterdir()} != before_top, "unexpected P029 top-level change")
    cleanup_source(data)
    receipt["receipt_sha256"] = digest(raw_receipt)
    receipt["ram_source_removed"] = True
    print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[3] != "--installer-sha":
        raise SystemExit("STOP explicit P030 manifest/installer hashes required")
    if sys.argv[1] == "--approved-p030-initdiag-v1":
        install(sys.argv[2], sys.argv[4])
    elif sys.argv[1] == "--resume-p030-initdiag-v1-receipt-only":
        resume_receipt(sys.argv[2], sys.argv[4])
    else:
        raise SystemExit("STOP unsupported P030 operation")
