#!/usr/bin/env python3
"""Host-only passive A/B/C packet; never writes canonical deployment gates."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
RELEASE = "6.1.99-rk3576-m0echo-p026"
BASE_SHA = "76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90"
BASE = ROOT / "artifacts/local/p026-kernel-isolated-clean"
DT_INPUT = ROOT / "artifacts/local/p023-dt-fit-clean-v3"
MBOX = ["/mailbox@2ae50000", "/mailbox@2ae54000"]
DISABLE = ["/rpmsg@47d00000", *MBOX]
INPUTS = {
    "Image": (BASE / "arch/arm64/boot/Image", "8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d"),
    "initrd": (BASE / f"initrd.img-{RELEASE}", "c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d"),
    "rk3576_amp_echo_test.ko": (BASE / "echo/rk3576_amp_echo_test.ko", "cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43"),
}


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1048576), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(*args):
    return subprocess.check_output([str(x) for x in args], text=True, stderr=subprocess.PIPE).strip()


def checked_copy(source, dest, expected):
    if sha(source) != expected:
        raise ValueError(f"source identity mismatch: {source}")
    shutil.copyfile(source, dest)
    assert sha(dest) == expected


def prop(dt, node, name, kind="s"):
    return run("fdtget", "-t", kind, dt, node, name)


def status(dt, node, value):
    run("fdtput", "-t", "s", dt, node, "status", value)


def boot_script(stage, boot, out):
    # U-Boot test integer comparison is decimal; filesize uses bare lowercase hex.
    lines = [f"echo P029 stage {stage} explicit test - factory default unchanged",
             'if part number mmc 0 boot p029_part; then true; else echo STOP missing boot partition; exit 1; fi',
             'if test "${p029_part}" = "2"; then true; else echo STOP unexpected boot partition; exit 1; fi']
    inputs = [("Image", 0x40400000), ("initrd", 0x4A200000), (f"stage-{stage}.dtb", 0x48300000)]
    for name, addr in inputs:
        size = (boot / name).stat().st_size
        path = f"/amp-p029/{name}"
        lines += [f'if size mmc 0:${{p029_part}} {path}; then true; else echo STOP size {name}; exit 1; fi',
                  f'if test "${{filesize}}" = "{size:x}"; then true; else echo STOP length {name}; exit 1; fi',
                  f'if load mmc 0:${{p029_part}} 0x{addr:x} {path} {size:x}; then true; else echo STOP load {name}; exit 1; fi',
                  f'if test "${{filesize}}" = "{size:x}"; then true; else echo STOP short read {name}; exit 1; fi']
    args = ("storagemedia=emmc androidboot.storagemedia=emmc androidboot.mode=normal "
            "root=/dev/mmcblk0p3 boot_part=2 earlyprintk console=ttyFIQ0 console=tty1 "
            "consoleblank=0 loglevel=7 rootwait rw rootfstype=ext4 "
            f"amp_test_stage={stage}")
    if stage == "C":
        args += ' dyndbg="file virtio_rpmsg_bus.c +p"'
    lines += [f"setenv bootargs '{args}'", "setenv fdt_high 0xffffffffffffffff", "setenv initrd_high 0xffffffffffffffff"]
    # After any M0 load attempt, never fall back to boot/default DT in this session.
    if stage != "A":
        lines += ['if amp_m0load /amp-p029/amp-host.itb 0x48300000; then',
                  '  echo P029 M0 loader returned success - immediately boot prepared Linux',
                  f'  booti 0x40400000 0x4a200000:{(boot / "initrd").stat().st_size:x} 0x48300000',
                  'else', '  echo STOP M0 attempt failed - cold power cycle required', 'fi',
                  'echo STOP no same-session boot or retry permitted', 'exit 1']
    else:
        lines += [f'booti 0x40400000 0x4a200000:{(boot / "initrd").stat().st_size:x} 0x48300000',
                  'echo STOP stage A boot returned', 'exit 1']
    cmd = out / f"stage-{stage}.cmd"
    cmd.write_text("\n".join(lines) + "\n")
    # P028's private factory SCRIPT compatibility requires PPC/Linux, not ARM64.
    run("mkimage", "-A", "ppc", "-O", "linux", "-T", "script", "-C", "none",
        "-n", f"P029 Stage {stage}", "-d", cmd, boot / f"stage-{stage}.scr")
    raw = (boot / f"stage-{stage}.scr").read_bytes()
    import zlib
    fields = struct.unpack(">7I4B32s", raw[:64])
    header = bytearray(raw[:64]); header[4:8] = bytes(4)
    assert fields[0] == 0x27051956 and fields[1] == zlib.crc32(header)
    assert fields[6] == zlib.crc32(raw[64:]) and fields[3] + 64 == len(raw)
    assert fields[7:11] == (5, 7, 6, 0)  # Linux/PPC/SCRIPT/none
    assert struct.unpack(">II", raw[64:72]) == (len(raw) - 72, 0)


def validate_dts(boot):
    for stage in "ABC":
        dt = boot / f"stage-{stage}.dtb"
        assert prop(dt, "/chosen", "project,p029-stage") == stage
        for node, base, size in [("mcu@47800000", "47800000", "80000"),
                                 ("rpmsg@47d00000", "47d00000", "10000"),
                                 ("rpmsg-dma@47d10000", "47d10000", "10000")]:
            path = "/reserved-memory/" + node
            assert prop(dt, path, "reg", "x") == f"0 {base} 0 {size}"
            assert prop(dt, path, "no-map", "x") == ""
            props = run("fdtget", "-p", dt, path).splitlines()
            assert "reusable" not in props
        assert prop(dt, "/mcu-amp", "status") == ("disabled" if stage == "A" else "okay")
        for node in DISABLE:
            assert prop(dt, node, "status") == ("okay" if stage == "C" else "disabled")
        assert prop(dt, "/serial@2ad80000", "status") == "disabled"
        assert prop(dt, "/i2c@2ac60000/ov8858-0@36", "status") == "okay"


def geometry(boot):
    raw = (boot / "Image").read_bytes()[:64]
    offset, image_size, flags = struct.unpack_from("<QQQ", raw, 8)
    assert raw[56:60] == b"ARM\x64" and image_size > 0
    dst = 0x40400000 - offset if flags & 8 else 0x40200000
    dst = (dst + 0x1FFFFF) // 0x200000 * 0x200000 + offset
    regions = [("Image-file", 0x40400000, (boot / "Image").stat().st_size),
               ("Image-runtime-source", 0x40400000, image_size), ("Image-relocation", dst, image_size),
               ("initrd", 0x4A200000, (boot / "initrd").stat().st_size),
               ("DT-with-padding", 0x48300000, 0x80000),
               ("script", 0x4C000000, 0x10000)]
    protected = [(0x47800000, 0x80000), (0x47D00000, 0x20000), (0x48400000, 0x1000000)]
    for name, start, length in regions:
        assert length > 0 and start + length <= 0x100000000
        assert all(start + length <= p or start >= p + n for p, n in protected), name
    return [{"name": n, "base": hex(b), "size": hex(s), "end_exclusive": hex(b+s)} for n,b,s in regions]


def make(output):
    output.mkdir(parents=True, exist_ok=False)
    boot = output / "boot"; boot.mkdir()
    for name, (source, digest) in INPUTS.items():
        checked_copy(source, boot/name, digest)
    module_tar = BASE / f"modules-{RELEASE}.tar"
    checked_copy(module_tar, output / "modules.tar", "413dd5260c8a664f40c996fa3b7e9139b5539d87046b64f1a5cee9bf838435e3")
    module_files = []
    with tarfile.open(module_tar) as archive:
        seen = set()
        for member in archive:
            path = Path(member.name)
            assert not path.is_absolute() and ".." not in path.parts
            assert path.parts[:3] == ("lib", "modules", RELEASE) and path not in seen
            seen.add(path)
            if member.isdir():
                continue
            assert member.isfile(), member.name
            module_files.append((hashlib.sha256(archive.extractfile(member).read()).hexdigest(), member.name))
    (output/"modules.sha256").write_text("".join(f"{h}  {n}\n" for h,n in module_files))
    fit_report = json.loads((ROOT/"artifacts/local/p029-fit-clean-v3/result.json").read_text())
    fit = ROOT / "artifacts/local/p029-fit-clean-v3/amp-host.itb"
    checked_copy(fit, boot/"amp-host.itb", fit_report["FIT"]["sha256"])
    kernel_dt = BASE / "arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dtb"
    assert sha(kernel_dt) == BASE_SHA  # paired kernel's base is byte-identical to factory v2 DT
    camera = output / "cam0.dtbo"
    factory = ROOT / "artifacts/local/p024-recovery-image/payloads/boot.payload"
    run("debugfs", "-R", f"dump /dtb/overlay/rk3576-lubancat-3-cam0-ov8858-1632x1224-30fps-overlay.dtbo {camera}", factory)
    assert camera.stat().st_size == 1996
    full = boot / "stage-C.dtb"
    run("fdtoverlay", "-i", kernel_dt, "-o", full, camera, DT_INPUT/"amp-host.dtbo")
    for stage in "ABC":
        dt = boot / f"stage-{stage}.dtb"
        if stage != "C":
            shutil.copyfile(full, dt)
            for node in DISABLE:
                status(dt, node, "disabled")
            if stage == "A":
                status(dt, "/mcu-amp", "disabled")
        run("fdtput", "-t", "s", dt, "/chosen", "project,p029-stage", stage)
        boot_script(stage, boot, output)
    validate_dts(boot)
    paths = geometry(boot)
    shutil.copyfile(ROOT/"scripts/board/p029_stage_assets.sh", output/"stage-assets.sh")
    shutil.copyfile(ROOT/"docs/reviews/rk3576-amp-platform-closure/P029_STAGED_TEST_GUIDE.md", output/"README.md")
    (output/"boot.sha256").write_text("".join(f"{sha(f)}  {f.name}\n" for f in sorted(boot.iterdir())))
    records = [{"path": str(f.relative_to(output)), "size": f.stat().st_size, "sha256": sha(f)}
               for f in sorted(output.rglob("*")) if f.is_file()]
    manifest = {"id": "P029", "status": "HOST_PREPARED_PENDING_STAGE_A_APPROVAL", "amp_level": "C. HOST_BUILD_PASS",
                "board_access": False, "board_test_authorized": False, "D_ready": False,
                "requires_board_uboot_sha256": "9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e",
                "uboot_image_included": False, "kernel_release": RELEASE,
                "contract_sha256": sha(ROOT/"docs/amp/AMP_PLATFORM_CONTRACT.yaml"),
                "rtos_base": "3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb",
                "hal_derived": "bc99978c1a030ad79610e89a5780dcd0ee3bb1f2",
                "diagnostic_patch_sha256": sha(ROOT/"patches/rk3576-amp-platform/0010-m0-bounded-runtime-evidence.patch"),
                "m0_binary_sha256": fit_report["payload_sha256"], "geometry": paths,
                "new_board_paths": ["/boot/amp-p029", f"/lib/modules/{RELEASE}"],
                "unchanged_default": ["/boot/boot.scr", "/boot/uEnv", "/boot/Image", "/boot/initrd", "/boot/rk-kernel.dtb"],
                "stages": {"A": "Linux reserves only; M0/RPMsg/mbox disabled", "B": "one M0 start; no Linux RPMsg", "C": "new cold boot; one M0 start; bounded manual echo KO"},
                "runtime_evidence": "UNVERIFIED", "files": records}
    (output/"MANIFEST.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False)+"\n")
    (output/"SHA256SUMS").write_text("".join(f"{sha(f)}  {f.relative_to(output)}\n" for f in sorted(output.rglob("*"))
                                            if f.is_file() and f.name != "SHA256SUMS"))
    return manifest


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    result = make(args.output.resolve())
    print(json.dumps({"status": result["status"], "files": len(result["files"]), "geometry": result["geometry"]}, indent=2))
