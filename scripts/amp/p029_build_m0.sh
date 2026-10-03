#!/usr/bin/env bash
# Host only. Fresh archive + diagnostic patch, preserving previous candidates.
set -euo pipefail
project=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:?fresh absolute output directory required}
[[ $out == /* ]] || { echo 'STOP: absolute Host output path required'; exit 2; }
rtos="$project/../p023-rtos"
hal="$project/../p023-hal"
base=3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb
toolchain=/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin
test ! -e "$out"
test "$(git -C "$hal" rev-parse HEAD)" = bc99978c1a030ad79610e89a5780dcd0ee3bb1f2
test -z "$(git -C "$hal" status --porcelain)"
mkdir -p "$out/source"
git -C "$rtos" archive "$base" | tar -xf - -C "$out/source"
python3 - "$out/source/bsp/rockchip/common/hal" "$hal" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
assert p.is_symlink()
p.unlink()
p.symlink_to(Path(sys.argv[2]).resolve())
PY
git -C "$out/source" apply --check "$project/patches/rk3576-amp-platform/0010-m0-bounded-runtime-evidence.patch"
git -C "$out/source" apply "$project/patches/rk3576-amp-platform/0010-m0-bounded-runtime-evidence.patch"
export RTT_ROOT="$out/source" RTT_EXEC_PATH="$toolchain"
cd "$out/source/bsp/rockchip/rk3576-mcu"
cp board/evb/defconfig .config
scons --useconfig=.config > "$out/config.log" 2>&1
scons -j4 > "$out/build.log" 2>&1
cp rtthread.elf rtthread.bin rtthread.map rtconfig.h "$out/"
"$toolchain/arm-none-eabi-readelf" -h -l "$out/rtthread.elf" > "$out/readelf.txt"
"$toolchain/arm-none-eabi-objdump" -d "$out/rtthread.elf" > "$out/disassembly.txt"
"$toolchain/arm-none-eabi-nm" -n "$out/rtthread.elf" > "$out/nm.txt"
sha256sum "$out/rtthread.elf" "$out/rtthread.bin" "$out/rtthread.map" > "$out/hashes.sha256"
echo 'P029_M0_HOST_BUILD_PASS; BOARD_NOT_RUN'
