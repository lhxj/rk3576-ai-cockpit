#!/usr/bin/env python3
"""Fresh Host object check for the U-Boot draft. Does not build/deploy firmware."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source, out = args.source.resolve(), args.output.resolve()
    if out.exists():
        raise SystemExit("Refusing to reuse an existing output directory")
    out.mkdir(parents=True)
    report = {"scope": "Host candidate objects only; no bootable image or board access",
              "source": str(source), "commands": [], "objects": []}

    def run(argv, label, cwd=source):
        result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, timeout=180)
        log = out / (label + ".log")
        log.write_text("STDOUT\n" + result.stdout + "\nSTDERR\n" + result.stderr)
        report["commands"].append({"command": argv, "cwd": str(cwd),
                                   "exit_code": result.returncode,
                                   "log": str(log), "log_sha256": sha(log)})
        (out / "object-build.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"{label}: exit {result.returncode}", flush=True)
        if result.returncode:
            print(result.stdout[-6000:] + result.stderr[-6000:])
            raise SystemExit(result.returncode)
        return result.stdout

    report["compiler"] = run(["aarch64-linux-gnu-gcc", "--version"], "compiler").splitlines()[0]
    report["base_sha"] = run(["git", "rev-parse", "HEAD"], "source-head").strip()
    run(["git", "diff", "--check"], "diff-check")
    make = ["make", f"O={out}", "CROSS_COMPILE=aarch64-linux-gnu-"]
    run(make + ["rk3576_defconfig"], "base-config")
    base = out / "rk3576-base.config"
    base.write_bytes((out / ".config").read_bytes())
    report["base_config_sha256"] = sha(base)
    fragment = out / "amp-host-only.fragment"
    fragment.write_text("CONFIG_AMP=y\nCONFIG_ROCKCHIP_AMP=y\n")
    # Reviewed upstream merge only, then canonical Kconfig regeneration.
    run(["bash", "scripts/kconfig/merge_config.sh", "-m", "-O", str(out),
         str(base), str(fragment)], "merge-config")
    run(make + ["olddefconfig"], "amp-config")
    config = (out / ".config").read_text()
    if "CONFIG_AMP=y\n" not in config or "CONFIG_ROCKCHIP_AMP=y\n" not in config:
        raise SystemExit("AMP options did not survive Kconfig regeneration")
    report["amp_config_sha256"] = sha(out / ".config")
    targets = ["arch/arm/mach-rockchip/rk3576/rk3576.o", "drivers/cpu/rockchip_amp.o",
               "arch/arm/mach-rockchip/board.o"]
    run(make + ["-j2"] + targets, "objects")
    for name in targets:
        obj = out / name
        header = run(["aarch64-linux-gnu-readelf", "-h", str(obj)], name.replace("/", "_") + "-header")
        if "AArch64" not in header or "REL (Relocatable file)" not in header:
            raise SystemExit(f"unexpected object architecture/type: {name}")
        report["objects"].append({"path": str(obj), "sha256": sha(obj),
                                  "architecture": "AArch64", "type": "relocatable object"})
    report["status"] = "HOST_OBJECT_CHECK_PASS"
    (out / "object-build.json").write_text(json.dumps(report, indent=2) + "\n")
    print("HOST_OBJECT_CHECK_PASS (not full U-Boot or board capability verification)")


if __name__ == "__main__":
    main()
