# Live ASR audio environment

Board: LubanCat-3 v2, RK3576, Debian 12 AArch64, kernel 6.1.99-rk3576. `hw:0,0` was enumerated as card 0 `rockchip-es8388`, device 0 `dailink-multicodecs ES8323 HiFi-0` on 2026-10-01. PulseAudio and PipeWire were active; the target capture node had no holder during T0/T1 prechecks. See [inventory](../asr/LIVE_AUDIO_ENVIRONMENT.md). Program/runtime/model are under `/home/cat/cockpit/asr-live` and `/home/cat/cockpit/asr-target`; no system audio settings changed.
