#!/usr/bin/env bash
# Explicit manual Stage 1 only. Never run by Host CI against a board.
# No reboot, MMIO, AMP start, GPT changes, or /boot writes.
set -Eeuo pipefail
umask 077

fail() { printf 'STOP: %s\n' "$*" >&2; exit 2; }
[[ $# == 2 ]] || fail 'usage: sudo bash p027_manual_uboot.sh --check|--write CANDIDATE.img'
amp_mode=$1
amp_input=$2
[[ "$amp_mode" == --check || "$amp_mode" == --write ]] || fail 'unknown mode'
[[ $EUID == 0 ]] || fail 'run through sudo; no software installation is needed'

readonly amp_candidate_sha=4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f
readonly amp_original_sha=ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a
readonly amp_result_sha=2f6d9a427bbde0d658fbd55e9617a8d119f4d9e442f400ca126a21a095c08eea
readonly amp_model_file=/sys/firmware/devicetree/base/model
readonly amp_label=/dev/disk/by-partlabel/uboot
readonly amp_expected_device=/dev/mmcblk0p1
readonly amp_start_file=/sys/class/block/mmcblk0p1/start
readonly amp_size_file=/sys/class/block/mmcblk0p1/size
readonly amp_boot=/boot
readonly amp_run=/run

for amp_command in cat readlink stat sha256sum blockdev findmnt uname tr cp dd cmp mktemp flock rm rmdir; do
    command -v "$amp_command" >/dev/null || fail "missing command: $amp_command"
done
[[ -f "$amp_input" && ! -L "$amp_input" ]] || fail 'candidate must be a regular, non-symlink file'
[[ $(stat -c %s -- "$amp_input") == 4194304 ]] || fail 'candidate size must be exactly 4MiB'
[[ $(tr -d '\000' < "$amp_model_file") == 'EmbedFire LubanCat-3-v2' ]] || fail 'wrong board model'
[[ $(uname -r) == 6.1.99-rk3576 ]] || fail 'Stage 1 expects the original Linux release'
[[ $(findmnt -n -o FSTYPE -T "$amp_run") == tmpfs ]] || fail '/run is not the expected RAM filesystem'
[[ $(findmnt -n -o SOURCE -T /) == /dev/mmcblk0p3 ]] || fail 'unexpected root filesystem device'
[[ $(findmnt -n -o SOURCE -T "$amp_boot") == /dev/mmcblk0p2 ]] || fail 'unexpected /boot filesystem device'

amp_device=$(readlink -e -- "$amp_label") || fail 'uboot partition label unavailable'
[[ "$amp_device" == "$amp_expected_device" && -b "$amp_device" ]] || fail 'unexpected uboot block device'
[[ $(cat "$amp_start_file") == 16384 ]] || fail 'unexpected partition start sector'
[[ $(cat "$amp_size_file") == 16384 ]] || fail 'unexpected partition length in sectors'
[[ $(blockdev --getss "$amp_device") == 512 ]] || fail 'unexpected logical sector size'
[[ $(blockdev --getsize64 "$amp_device") == 8388608 ]] || fail 'partition must be exactly 8MiB'
[[ $(blockdev --getro "$amp_device") == 0 ]] || fail 'block device is read-only'
if findmnt -rn -S "$amp_device" >/dev/null; then fail 'uboot partition is mounted'; fi

# These locks do not claim to exclude unrelated users/tools with root access.
exec 9>"$amp_run/lock/rk3576-p027-uboot.lock"
flock -n 9 || fail 'another instance holds the U-Boot operation lock'
amp_tmp=$(mktemp -d "$amp_run/rk3576-p027.XXXXXX")
amp_write_started=0
amp_verified=0
amp_cleanup() {
    local amp_exit=$?
    if [[ $amp_write_started == 1 && $amp_verified != 1 ]]; then
        printf 'WRITE/VERIFY FAILED: keep Linux running; do not reboot/power off or retry. Preserve this output for recovery review.\n' >&2
    fi
    rm -f -- "$amp_tmp/candidate.img" "$amp_tmp/before.raw" "$amp_tmp/after.raw"
    rmdir -- "$amp_tmp" || true
    return "$amp_exit"
}
trap amp_cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Stage the verified image under a private root-owned directory before writing.
cp -- "$amp_input" "$amp_tmp/candidate.img"
[[ $(stat -c %s -- "$amp_tmp/candidate.img") == 4194304 ]] || fail 'staged size changed'
amp_sha_file() { local amp_sum; amp_sum=$(sha256sum -- "$1") || return; printf '%s' "${amp_sum%% *}"; }
[[ $(amp_sha_file "$amp_tmp/candidate.img") == "$amp_candidate_sha" ]] || fail 'candidate SHA256 mismatch'

[[ $(readlink -- "$amp_boot/Image") == Image-6.1.99-rk3576 ]] || fail 'Image link is not the original'
[[ $(readlink -- "$amp_boot/initrd") == initrd.img-6.1.99-rk3576 ]] || fail 'initrd link is not the original'
[[ $(readlink -- "$amp_boot/rk-kernel.dtb") == dtb/rk3576-lubancat-3-v2.dtb ]] || fail 'DTB link is not the original'
[[ $(amp_sha_file "$amp_boot/Image") == e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e ]] || fail 'original Image SHA mismatch'
[[ $(amp_sha_file "$amp_boot/initrd") == 425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397 ]] || fail 'original initrd SHA mismatch'
[[ $(amp_sha_file "$amp_boot/rk-kernel.dtb") == 76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90 ]] || fail 'original DTB SHA mismatch'
[[ $(amp_sha_file "$amp_boot/uEnv/uEnv.txt") == 4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64 ]] || fail 'uEnv SHA mismatch'
[[ $(amp_sha_file "$amp_boot/boot.scr") == c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325 ]] || fail 'boot.scr SHA mismatch'
[[ $(amp_sha_file "$amp_boot/boot.cmd") == 9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55 ]] || fail 'boot.cmd SHA mismatch'

dd if="$amp_device" of="$amp_tmp/before.raw" bs=1M count=8 iflag=fullblock status=none
[[ $(stat -c %s -- "$amp_tmp/before.raw") == 8388608 ]] || fail 'short pre-write partition read'
[[ $(amp_sha_file "$amp_tmp/before.raw") == "$amp_original_sha" ]] || fail 'current U-Boot is not the backed-up original; no write'
printf 'CHECK_PASS device=%s partition_bytes=8388608 write_bytes=4194304 start_sector=16384\n' "$amp_device"
printf 'candidate_sha256=%s\noriginal_partition_sha256=%s\n' "$amp_candidate_sha" "$amp_original_sha"
if [[ "$amp_mode" == --check ]]; then
    printf 'CHECK_ONLY: no eMMC write, no reboot, no M0 start.\n'
    exit 0
fi

printf 'Writing only the first 4MiB of the verified uboot partition.\n'
amp_write_started=1
dd if="$amp_tmp/candidate.img" of="$amp_device" bs=1M count=4 iflag=fullblock conv=notrunc,fsync status=progress

# Fresh device open and direct block read avoid treating the writer cache as readback.
dd if="$amp_device" of="$amp_tmp/after.raw" bs=1M count=8 iflag=fullblock,direct status=none
[[ $(stat -c %s -- "$amp_tmp/after.raw") == 8388608 ]] || fail 'short readback'
cmp -n 4194304 -- "$amp_tmp/candidate.img" "$amp_tmp/after.raw" || fail 'first 4MiB readback mismatch'
cmp -i 4194304 -- "$amp_tmp/before.raw" "$amp_tmp/after.raw" || fail 'last 4MiB changed'
[[ $(amp_sha_file "$amp_tmp/after.raw") == "$amp_result_sha" ]] || fail 'full 8MiB readback SHA mismatch'
amp_verified=1
printf 'WRITE_VERIFIED\npartition_sha256=%s\n' "$amp_result_sha"
printf 'Linux is still running. No automatic reboot or M0 start. Save this output before a separately approved cold boot.\n'
