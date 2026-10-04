#!/usr/bin/env bash
# Full Host build only. No SDK packaging binaries, firmware execution or SSH.
set -euo pipefail
if (( $# != 3 )) || [[ "$3" != enabled && "$3" != disabled ]]; then
    echo 'Usage: build_uboot_fit_policy_host.sh SOURCE NEW_OUTPUT enabled|disabled' >&2
    exit 2
fi
src=$(realpath -- "$1")
out=$(realpath -m -- "$2")
test -f "$src/drivers/cpu/rockchip_amp.c"
if [[ -e "$out" ]]; then
    echo 'Output must be a new directory' >&2
    exit 2
fi
mkdir -p -- "$out"
aarch64-linux-gnu-gcc --version >"$out/compiler.txt"
make -C "$src" O="$out" CROSS_COMPILE=aarch64-linux-gnu- rk3576_defconfig >"$out/config.log" 2>&1
cp -- "$out/.config" "$out/base.config"
printf 'CONFIG_AMP=y\nCONFIG_ROCKCHIP_AMP=y\n' >"$out/fragment"
if [[ "$3" == enabled ]]; then
    printf 'CONFIG_FIT_SIGNATURE=y\n' >>"$out/fragment"
else
    printf '# CONFIG_FIT_SIGNATURE is not set\n' >>"$out/fragment"
fi
(
    cd "$src"
    bash scripts/kconfig/merge_config.sh -m -O "$out" "$out/base.config" "$out/fragment" >"$out/merge.log" 2>&1
)
make -C "$src" O="$out" CROSS_COMPILE=aarch64-linux-gnu- olddefconfig >"$out/olddefconfig.log" 2>&1
make -C "$src" O="$out" CROSS_COMPILE=aarch64-linux-gnu- -j4 u-boot.bin u-boot.dtb >"$out/build.log" 2>&1
aarch64-linux-gnu-readelf -h -l "$out/u-boot" >"$out/readelf.txt"
aarch64-linux-gnu-nm "$out/u-boot" >"$out/nm.txt"
sha256sum "$out/u-boot" "$out/u-boot.bin" "$out/u-boot.dtb" "$out/.config" >"$out/hashes.sha256"
echo 'HOST_UBOOT_POLICY_BUILD_PASS; not a packaged/signed deployment image'
