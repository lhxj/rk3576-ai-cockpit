#!/usr/bin/env bash
# Rebuild the unchanged, blocked candidate on Host. No board access.
set -euo pipefail
project=/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project
rtos=/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/rtos
bsp="$rtos/bsp/rockchip/rk3576-mcu"
out="$project/artifacts/local/amp-platform-candidate-build"
toolchain=/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin
mkdir -p "$out" "$out/fit"
export RTT_ROOT="$rtos" RTT_EXEC_PATH="$toolchain"
"$toolchain/arm-none-eabi-gcc" --version > "$out/compiler-version.txt"
scons --version > "$out/scons-version.txt"
mkimage -V > "$out/mkimage-version.txt" 2>&1
cd "$bsp"
scons -c > "$out/scons-clean.log" 2>&1
scons -j4 > "$out/scons-build.log" 2>&1
cp rtthread.elf rtthread.map rtthread.bin "$out/"
cp Image/amp.its "$out/fit/amp.its"
cp rtthread.bin "$out/fit/rtt.bin"
cd "$out/fit"
mkimage -f amp.its -E -p 0xe00 amp.img > "$out/mkimage.log" 2>&1
mkimage -l amp.img > "$out/fit-inspection.txt" 2>&1
file "$out/rtthread.elf" "$out/rtthread.bin" "$out/fit/amp.img" > "$out/file.txt"
"$toolchain/arm-none-eabi-readelf" -h "$out/rtthread.elf" > "$out/readelf-h.txt"
"$toolchain/arm-none-eabi-readelf" -S "$out/rtthread.elf" > "$out/readelf-S.txt"
"$toolchain/arm-none-eabi-readelf" -l "$out/rtthread.elf" > "$out/readelf-l.txt"
"$toolchain/arm-none-eabi-size" "$out/rtthread.elf" > "$out/size.txt"
"$toolchain/arm-none-eabi-nm" -n "$out/rtthread.elf" > "$out/nm.txt"
"$toolchain/arm-none-eabi-objdump" -h "$out/rtthread.elf" > "$out/objdump-h.txt"
sha256sum "$out/rtthread.elf" "$out/rtthread.map" "$out/rtthread.bin" "$out/fit/amp.img" > "$out/sha256.txt"
printf '%s\n' 'HOST_CANDIDATE_BUILD_PASS; NOT DEPLOYABLE'
