#!/usr/bin/env python3
"""Prepare and object-check a one-flag Linux transport draft on Host only."""

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

SOURCE_SHA = "7b971ef22788c6fd21e1119e93174aaa05b0aa03090fea43e89690770f909071"
KERNEL_COMMIT = "521833e2d28decbd6473d5717f1f96cc4108e208"
CONFIG_SHA = "f1f1dad7b2987c425b2807919952c57176a56f660cb1b5a5dd662893513191ee"
PRIVATE_HEADER_SHA = "cd253b8f21ddbe7dc382b23a1907886c12cd3610c3bb56eb202401222486554a"
SOURCE_PATH = "drivers/rpmsg/rockchip_rpmsg_mbox.c"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--transport", type=Path, required=True)
    parser.add_argument("--private-header", type=Path, required=True)
    parser.add_argument("--headers", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--patch", type=Path, required=True)
    args = parser.parse_args()
    if sha(args.transport) != SOURCE_SHA:
        raise SystemExit("unexpected fixed transport source hash")
    if sha(args.headers / ".config") != CONFIG_SHA:
        raise SystemExit("unexpected copied board header config hash")
    if sha(args.private_header) != PRIVATE_HEADER_SHA:
        raise SystemExit("unexpected fixed kernel private header hash")
    if args.output.exists():
        raise SystemExit("refusing to reuse existing output directory")
    out = args.output.resolve()
    out.mkdir(parents=True)
    original = args.transport.read_text()
    marker = "RPMSG_VRING_ALIGN, vdev, true, ctx,"
    if original.count(marker) != 1:
        raise SystemExit("transport queue call changed")
    modified = original.replace(marker, "RPMSG_VRING_ALIGN, vdev, false, ctx,")
    # Preserve the entire fixed source byte-for-byte except for this flag.
    (out / "rockchip_rpmsg_mbox.c").write_text(modified)
    shutil.copyfile(args.private_header, out / "rpmsg_internal.h")
    (out / "Makefile").write_text("obj-m += rockchip_rpmsg_mbox.o\n")
    patch = "".join(difflib.unified_diff(original.splitlines(True), modified.splitlines(True),
                                       fromfile="a/" + SOURCE_PATH, tofile="b/" + SOURCE_PATH))
    args.patch.parent.mkdir(parents=True, exist_ok=True)
    args.patch.write_text(patch)
    report = {"scope": "Host AArch64 object only; no ko or kernel image; not deployable",
              "source_commit": KERNEL_COMMIT, "source_sha256": SOURCE_SHA,
              "patch_sha256": sha(args.patch), "modified_sha256": sha(out / "rockchip_rpmsg_mbox.c"),
              "private_header_sha256": sha(args.private_header),
              "header_config_sha256": CONFIG_SHA, "commands": []}

    def run(argv, label):
        result = subprocess.run(argv, capture_output=True, text=True, timeout=60)
        log = out / (label + ".log")
        log.write_text("STDOUT\n" + result.stdout + "\nSTDERR\n" + result.stderr)
        report["commands"].append({"command": argv, "exit_code": result.returncode,
                                   "log_sha256": sha(log)})
        (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"{label}: exit {result.returncode}", flush=True)
        if result.returncode:
            print(result.stdout[-5000:] + result.stderr[-5000:])
            raise SystemExit(result.returncode)
        return result.stdout

    report["compiler"] = run(["aarch64-linux-gnu-gcc", "--version"], "compiler").splitlines()[0]
    run(["make", "-C", str(args.headers.resolve()), f"M={out}", "ARCH=arm64",
         "CROSS_COMPILE=aarch64-linux-gnu-", "HOSTCC=gcc", "rockchip_rpmsg_mbox.o"], "build")
    obj = out / "rockchip_rpmsg_mbox.o"
    header = run(["aarch64-linux-gnu-readelf", "-h", str(obj)], "readelf")
    if "AArch64" not in header or "REL (Relocatable file)" not in header:
        raise SystemExit("unexpected object architecture/type")
    report["object_sha256"] = sha(obj)
    report["status"] = "HOST_OBJECT_CHECK_PASS; EXACT_KERNEL_SOURCE_MATCH_UNVERIFIED"
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"])


if __name__ == "__main__":
    main()
