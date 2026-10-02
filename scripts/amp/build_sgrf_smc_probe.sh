#!/usr/bin/env bash
# Host-only build against headers copied from the running LubanCat kernel.
set -euo pipefail
project=/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project
headers="$project/artifacts/local/amp-kernel-headers-20261001T134544Z-137756/linux-headers-6.1.99-rk3576"
source_dir="$project/scripts/board/amp_sgrf_smc_probe"
out_root="$project/artifacts/local"
test -f "$headers/Module.symvers"
test -f "$headers/include/generated/autoconf.h"
test -x "$headers/scripts/basic/fixdep"
test -x "$headers/scripts/mod/modpost"
mkdir -p "$out_root"
out=$(mktemp -d "$out_root/sgrf-smc-probe-XXXXXXXX")
cp "$source_dir/Makefile" "$source_dir/rk3576_sgrf_smc_probe.c" "$out/"
aarch64-linux-gnu-gcc --version > "$out/compiler-version.txt"
make -C "$headers" M="$out" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- HOSTCC=gcc modules > "$out/build.log" 2>&1
file "$out/rk3576_sgrf_smc_probe.ko" > "$out/file.txt"
modinfo "$out/rk3576_sgrf_smc_probe.ko" > "$out/modinfo.txt"
sha256sum "$out/rk3576_sgrf_smc_probe.ko" > "$out/sha256.txt"
printf '%s\n' "$out"
