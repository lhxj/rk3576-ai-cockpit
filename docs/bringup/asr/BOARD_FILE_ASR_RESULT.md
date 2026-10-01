# RK3576 board file ASR validation result

**Result: `ASR_BOARD_FILE_RECOGNITION_PASS` — file input only.** Observed on 2026-10-01 on `agent/asr-rk3576-file-validation`, based on product commit `052a60e`. The same main-project `tools/asr_file_test/main.cpp` invokes `read_pcm_wav → SherpaAsrBackend` through `IAsrBackend` → `VoiceSessionController::deliver_event` → typed `ASR_FINAL`. No official demo executable, real audio device, microphone, or other product service was used.

| Acceptance evidence | Result |
| --- | --- |
| Runtime and target | Official Sherpa-ONNX v1.11.3 AArch64 CPU shared release with ONNX Runtime 1.17.1. Product C++17 tool and cancel test compiled natively by target GCC 12.2. `file` showed AArch64 PIE ELF and AArch64 shared libraries; target `ldd` resolved every NEEDED, `--help` exited 0. |
| Model identity | Five configured model assets totaling 60,387,707 bytes. Host SHA256 and target `sha256sum -c` matched for every asset; see manifest below. No model was committed. |
| WAV identity | Same local reference `test_wavs/0.wav`, 16 kHz mono S16 PCM, 10.0531 s. SHA256 on Host and target: `7d93384ca14702cc584a7a33fe2fed92e89e708549161cb12ea38c916882103b`. |
| Model load | `--load-only` exited 0, `MODEL_LOADED load_ms=7617.91`. Full recognition run loaded in 7,871 ms. |
| Actual result | Board: `ASR_FINAL 昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`. Host baseline: identical text. Partial events were also observed, but final text was neither substituted nor hardcoded. |
| Decode | Board single file 1.73939 s, RTF 0.17302. Host baseline RTF approximately 0.024; this is a different CPU and not a board target threshold. |
| Cancel/session | Real Sherpa engine cancel test returned `SHERPA_CANCEL_SESSION_PASS`: old token produced no accepted FINAL, new session produced one FINAL. |
| Repeat | One process emitted one `MODEL_LOADED`, then three `ASR_FINAL` events for the same WAV. Decode times 1.70992, 1.70431, 1.70699 s. End-of-iteration RSS/PSS was stable in this short run; no long-duration leak claim. |
| Negative paths | Wrong model directory: exit 3 and `MODEL_NOT_FOUND`; missing WAV: exit 5; generated 8 kHz WAV: exit 5 and `UNSUPPORTED_SAMPLE_RATE`. Every case exited naturally. No residual task process was observed. |
| Host regressions | `bash scripts/dev/host_ci.sh`: 9/9 CTest and 6/6 Python passed. Explicit x86 Sherpa C API integration: 2/2 (`sherpa_file_integration`, `sherpa_cancel_integration`) passed. Default Host build still has Sherpa OFF. |

Configured model identity (the same digests in `config/models/asr/sherpa_file_asr.conf`):

| Asset | SHA256 |
| --- | --- |
| `encoder-epoch-99-avg-1.int8.onnx` | `db6f51551762e40e549166fe041ea3e45464370b595e9ad23f06478ec3794fbb` |
| `decoder-epoch-99-avg-1.onnx` | `89be509a83175261695bdef5fd1c7b9ab1129a663d1284e7ba9f8507b21e0906` |
| `joiner-epoch-99-avg-1.int8.onnx` | `bdda356d6f9b8c2d7cee9ee0e26075fa537490f7fd06520be408d287073667b9` |
| `tokens.txt` | `a8e0e4ec53810e433789b54a5c0134a7eaa2ffca595a6334d54c00da858841d3` |
| `bpe.model` | `bcae393dbc5611be5ffa4c7ae0841558978a5a4f484008cb9dff3a2cc97ebe01` |

The board package, model, and WAV live only below `/home/cat/cockpit/asr-target`. Raw local logs are ignored under `build/asr-target-tests/` (`load-only-20261001T132412Z.log`, `full-20261001T132435Z.log`). `docs/bringup/asr/PERFORMANCE.md` gives process and resource details. Model conversion package and test WAV remain `LICENSE_UNVERIFIED_FOR_DISTRIBUTION`; board success does not grant redistribution rights. Real-time capture, VAD/wake, runtime cancellation latency for long audio, and combined vision/voice resource behavior remain separate work.
