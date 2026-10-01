#!/usr/bin/env bash
# Read-only ALSA and development-header inventory for the live ASR task.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -u
echo '=== CARDS ==='
cat /proc/asound/cards
echo '=== CAPTURE DEVICES ==='
arecord -l
echo '=== PLAYBACK ENUMERATION ONLY ==='
aplay -l
echo '=== AUDIO SERVICES ==='
ps -ef | grep -E '[p]ipewire|[p]ulseaudio|[w]ireplumber' || true
echo '=== OPTIONAL AUDIO SERVICE STATUS ==='
if command -v pactl >/dev/null 2>&1; then pactl info | grep -iv 'cookie' || true; fi
if command -v wpctl >/dev/null 2>&1; then wpctl status | grep -iv 'cookie' || true; fi
echo '=== DEVICE OWNERS ==='
fuser /dev/snd/* 2>/dev/null || true
echo '=== ALSA DEVELOPMENT PACKAGE ==='
if dpkg -s libasound2-dev >/dev/null 2>&1; then echo 'libasound2-dev=INSTALLED'; else echo 'libasound2-dev=MISSING'; fi
if test -f /usr/include/alsa/asoundlib.h; then echo 'asoundlib.h=PRESENT'; else echo 'asoundlib.h=MISSING'; fi
if command -v pkg-config >/dev/null 2>&1; then pkg-config --modversion alsa || true; fi
echo '=== TARGET CAPTURE NODE ==='
ls -l /dev/snd/pcmC0D0c 2>&1 || true
echo '=== TASK OCCUPANCY ==='
ps -eo pid,comm | grep -E 'cockpit_(asr|live)' || true
REMOTE
