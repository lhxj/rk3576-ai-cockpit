#!/usr/bin/env python3
"""Fetch pinned upstream source files into ignored Host evidence, without running them."""
import base64
import hashlib
import json
import urllib.error
import urllib.request
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
DEST = PROJECT / "artifacts/local/amp-platform-sources"
PINS = {
    "LubanCat/u-boot": (
        "8f53f800da2c25d0c6ba414fb45902a01675703a",
        ["arch/arm/mach-rockchip/board.c",
         "arch/arm/mach-rockchip/rk3576/rk3576.c",
         "drivers/cpu/rockchip_amp.c",
         "configs/rk3576_defconfig"],
    ),
    "rockchip-linux/kernel": (
        "77168c8d5ab82399f65a80e9f807b50ba37cf483",
        ["arch/arm64/boot/dts/rockchip/rk3576-amp.dtsi",
         "drivers/rpmsg/rockchip_rpmsg_mbox.c",
         "drivers/soc/rockchip/rockchip_amp.c",
         "include/linux/rpmsg/rockchip_rpmsg.h"],
    ),
    "LubanCat/kernel": (
        "521833e2d28decbd6473d5717f1f96cc4108e208",
        ["arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dts",
         "drivers/rpmsg/rockchip_rpmsg_mbox.c",
         "include/linux/rpmsg/rockchip_rpmsg.h"],
    ),
}


def main() -> None:
    DEST.mkdir(parents=True, exist_ok=True)
    manifest = []
    for repo, (sha, paths) in PINS.items():
        for path in paths:
            url = f"https://api.github.com/repos/{repo}/contents/{path}?ref={sha}"
            req = urllib.request.Request(url, headers={"User-Agent": "amp-platform-static-audit"})
            try:
                with urllib.request.urlopen(req, timeout=15) as response:
                    item = json.load(response)
                data = base64.b64decode(item["content"])
                target = DEST / repo / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                record = {"repo": repo, "source_commit": sha, "path": path,
                          "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                          "github_blob_sha": item["sha"]}
            except (urllib.error.URLError, KeyError, ValueError) as exc:
                record = {"repo": repo, "source_commit": sha, "path": path,
                          "error": str(exc)}
            manifest.append(record)
            print(record)
    (DEST / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
