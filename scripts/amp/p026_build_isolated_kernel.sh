#!/usr/bin/env bash
# Host-only fresh kernel/module release; never mix stock modules with a new Image.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
src="$ROOT/artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208"
out="$ROOT/artifacts/local/p026-kernel-isolated-clean"
[[ $# == 0 && ! -e "$out" ]] || { echo 'Refusing reused build directory' >&2; exit 2; }
mkdir -p -- "$out"
cp -- "$ROOT/artifacts/local/p023-kernel-full-clean-v3/board-original.config" "$out/board-original.config"
cp -- "$out/board-original.config" "$out/.config"
# Use Kconfig's official editing/generation flow, not a second hand-maintained config.
"$src/scripts/config" --file "$out/.config" --set-str LOCALVERSION '-rk3576-m0echo-p026' --disable LOCALVERSION_AUTO
deps="$ROOT/artifacts/local/p023-standard-tools/root"
export PATH="$deps/usr/bin:$PATH"
export BISON_PKGDATADIR="$deps/usr/share/bison"
export M4="$deps/usr/bin/m4"
export CPATH="$deps/usr/include"
export LIBRARY_PATH="$deps/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$deps/usr/lib/x86_64-linux-gnu"
aarch64-linux-gnu-gcc --version >"$out/compiler.txt"
make -C "$src" O="$out" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig >"$out/config.log" 2>&1
make -C "$src" O="$out" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j6 Image modules rockchip/rk3576-lubancat-3-v2.dtb >"$out/build.log" 2>&1
test "$(cat "$out/include/config/kernel.release")" = 6.1.99-rk3576-m0echo-p026
mkdir -- "$out/echo"
cp -- "$ROOT/scripts/board/amp_echo_linux/Makefile" "$ROOT/scripts/board/amp_echo_linux/rk3576_amp_echo_test.c" "$out/echo/"
make -C "$src" O="$out" M="$out/echo" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules >"$out/echo/build.log" 2>&1
mkdir -- "$out/module-root"
# Install only into a new Host staging root, never /lib/modules on either machine.
make -C "$src" O="$out" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH="$out/module-root" DEPMOD=true modules_install >"$out/module-install.log" 2>&1
depmod -b "$out/module-root" -F "$out/System.map" 6.1.99-rk3576-m0echo-p026 >"$out/depmod.log" 2>&1
tar --sort=name --mtime='2026-10-03 00:00:00Z' --owner=0 --group=0 --numeric-owner \
    --exclude='lib/modules/6.1.99-rk3576-m0echo-p026/build' \
    --exclude='lib/modules/6.1.99-rk3576-m0echo-p026/source' \
    -C "$out/module-root" -cf "$out/modules-6.1.99-rk3576-m0echo-p026.tar" lib/modules/6.1.99-rk3576-m0echo-p026
printf 'Host fresh build PASS; isolated release: 6.1.99-rk3576-m0echo-p026\n'
sha256sum "$out/arch/arm64/boot/Image" "$out/echo/rk3576_amp_echo_test.ko" "$out/.config" "$out/Module.symvers" "$out/modules-6.1.99-rk3576-m0echo-p026.tar" >"$out/hashes.sha256"
