#!/usr/bin/env bash
# Read-only remote inventory; all output stays local and Git-ignored.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/inventory-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
set +e
timeout --signal=TERM --kill-after=2s 45s ssh "${SSH_ARGS[@]}" lubancat 'sh -s' >"$out/inventory.txt" 2>&1 <<'REMOTE'
section() { printf '\n===== %s =====\n' "$1"; }
section IDENTITY
id
uname -a
cat /etc/os-release
section RESOURCES
free -h
lsblk
df -h / /boot
cat /sys/devices/system/cpu/online
section UENV_READ_ONLY
ls -l /boot/uEnv/uEnv.txt
readlink -f /boot/uEnv/uEnv.txt
grep -nE '^[[:space:]]*dtoverlay=.*cam[0-4]|^enable_uboot_overlays=' /boot/uEnv/uEnv.txt
section VIDEO_NODE_NAMES
for p in /sys/class/video4linux/video*; do
    [ -e "$p/name" ] || continue
    printf '%s: ' "${p##*/}"
    cat "$p/name"
done
ls -l /dev/video-camera* /dev/media* 2>/dev/null
section AUDIO_ENUMERATION
if command -v arecord >/dev/null 2>&1; then arecord -l; else echo 'arecord missing'; fi
if command -v aplay >/dev/null 2>&1; then aplay -l; else echo 'aplay missing'; fi
section DISPLAY_CONNECTORS
for p in /sys/class/drm/*/status; do
    [ -r "$p" ] || continue
    printf '%s: ' "$p"
    cat "$p"
done
if command -v loginctl >/dev/null 2>&1; then loginctl list-sessions --no-pager; fi
section NETWORK_STATE_NO_SSID
if command -v nmcli >/dev/null 2>&1; then nmcli -t -f DEVICE,TYPE,STATE device status; fi
section RPMSG_CONFIG
cfg="/boot/config-$(uname -r)"
if [ -r "$cfg" ]; then
    grep -E 'CONFIG_(RPMSG|REMOTEPROC|MAILBOX|ROCKCHIP_MBOX|ROCKCHIP_AMP)' "$cfg"
else
    echo 'SKIP: running kernel config is not readable here'
fi
section AVAILABLE_TOOLS
for c in gcc g++ cmake qmake qmake6 qtpaths gst-launch-1.0 gst-inspect-1.0 ffmpeg v4l2-ctl media-ctl; do
    command -v "$c" 2>/dev/null || true
done
section DMESG_TAIL_READ_ONLY
if dmesg >/dev/null 2>&1; then dmesg | tail -n 120; else echo 'SKIP: dmesg not permitted; no automatic sudo'; fi
section INVENTORY_DONE
exit 0
REMOTE
rc=$?
set -e
printf 'ssh_or_timeout_exit_code=%s\n' "$rc" >"$out/result.txt"
printf 'Local evidence: %s\n' "$out"
printf 'Private raw evidence: do not commit or upload without redaction.\n'
if [[ "$rc" -ne 0 ]]; then
    printf 'Read-only inventory failed/timed out (%s). No password or privilege fallback used.\n' "$rc" >&2
fi
exit "$rc"
