#!/usr/bin/env bash
# Host-only module build against headers copied read-only from the running board.
set -euo pipefail
project=/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project
headers="$project/artifacts/local/amp-kernel-headers-20261001T134544Z-137756/linux-headers-6.1.99-rk3576"
out="$project/artifacts/local/amp-linux-echo-final"
test -f "$headers/Module.symvers"
test -f "$headers/include/generated/autoconf.h"
test -x "$headers/scripts/basic/fixdep"
test -x "$headers/scripts/mod/modpost"
mkdir -p "$out"
cp "$project/scripts/board/amp_echo_linux/Makefile" "$project/scripts/board/amp_echo_linux/rk3576_amp_echo_test.c" "$out/"
aarch64-linux-gnu-gcc --version > "$out/compiler-version.txt"
make -C "$headers" M="$out" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- HOSTCC=gcc modules > "$out/build.log" 2>&1
file "$out/rk3576_amp_echo_test.ko" > "$out/file.txt"
modinfo "$out/rk3576_amp_echo_test.ko" > "$out/modinfo.txt"
sha256sum "$out/rk3576_amp_echo_test.ko" > "$out/sha256.txt"
printf '%s\n' 'BOARD_HEADER_HOST_MODULE_BUILD_PASS; EXACT_SOURCE_COMMIT_UNVERIFIED'
