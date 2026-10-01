# RK3576 live audio inventory — 2026-10-01

Read-only `scripts/board/inventory_asr_live_audio.sh` through the existing `lubancat` SSH alias observed card 0 `rockchip-es8388`, capture device 0 `dailink-multicodecs ES8323 HiFi-0`, and `/dev/snd/pcmC0D0c`. Card 1 is HDMI; card 2 is DP. PulseAudio and PipeWire processes run. `fuser` found no holder on the capture PCM at the inspection time; this is rechecked before each gate. No service was stopped and no ALSA configuration was changed.

`libasound2-dev` and `/usr/include/alsa/asoundlib.h` were initially missing. The user explicitly approved installing only Debian 12 arm64 `libasound2-dev` 1.2.8-1+b1. `sudo -n apt-get install --no-install-recommends -y libasound2-dev` installed one new package, no upgrades or removals, and the header check passed. No other package or runtime configuration was installed.
