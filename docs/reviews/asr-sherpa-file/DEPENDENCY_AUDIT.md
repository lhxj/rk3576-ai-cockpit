# ASR-01 Sherpa file dependency audit

Evidence baseline: `superxiaobai-1/LLM_Voice_Flow` at `be82e87cc334ae6e222f83f7555531d1ddebaa8b`, read-only local path `/home/ywx/rk3576-work/reference/LLM_Voice_Flow`. The containing project remains `REFERENCE_ONLY`; its microphone and ZeroMQ integration is not used here.

| Item | Observed result |
| --- | --- |
| Sherpa version | Vendored `voice/sherpa-onnx/CMakeLists.txt:17` declares `SHERPA_ONNX_VERSION 1.11.3`. No independently pinned upstream Git commit was found in the containing Git tree; version is a source declaration, not a verified upstream revision. |
| Sherpa code license | `voice/sherpa-onnx/LICENSE` contains Apache-2.0, matching the official [v1.11.3 upstream license](https://github.com/k2-fsa/sherpa-onnx/blob/v1.11.3/LICENSE). This supports use of upstream Sherpa code; it does not automatically license the containing project's author-modified microphone glue or model weights. The vendored tree has no independent upstream commit marker, so review provenance/diffs before redistributing any vendored build. |
| Required runtime | Sherpa online recognizer and ONNX Runtime CPU. The vendored CMake's x86_64 shared runtime recipe requests a patched ONNX Runtime 1.17.1 archive (`cmake/onnxruntime-linux-x86_64.cmake`). The official prebuilt v1.11.3 C API library was linked and dynamically needs its adjacent `libonnxruntime.so`; its `OrtGetApiBase()->GetVersionString()` returned `1.17.1` on Host. No RKNN in this path. ONNX Runtime upstream [LICENSE](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) is MIT; the exact prebuilt bundle's notice/third-party obligations still need release review. |
| Optional libraries | PortAudio is enabled by Sherpa's default option but only entered with its binary examples (`CMakeLists.txt:38,371`); turn it OFF for the file backend. GPU, websocket, TTS, Python and example binaries are unnecessary. WAV PCM decoding is implemented locally with the C++ standard library. |
| Host | Ubuntu 22.04 WSL x86_64. `pkg-config sherpa-onnx` found no installed package; `/home/ywx/rk3576-work/deps` did not exist at audit time. Official v1.11.3 Host archive was acquired through `gh release download` exclusively into ignored main-repo `build/asr-deps/`; no global install or reference-tree edit. Archive size 19,346,494 bytes, SHA256 `84ce8e14e4d4aa692f8ecd9745b5a73a4b9103809cadbb7e645c0b5c9ea853bb`. GitHub API did not provide a published asset digest for independent comparison. |
| Build method | Main project option `COCKPIT_ENABLE_SHERPA_ASR=OFF` by default. ON requires an explicit external Sherpa include directory and library through a CMake imported target; absent dependency fails configure clearly. Official release package (Apache-2.0 Sherpa code, x86_64 ELF) linked in an ignored build tree. Runtime contains `libsherpa-onnx-c-api.so` (3,742,520 bytes) and `libonnxruntime.so` (15,534,400 bytes). Nothing is installed into `/usr/local` or tracked in Git. |
| Model | `sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16`, Chinese/English online transducer, FP32 and INT8 encoder/decoder/joiner variants plus `tokens.txt` and `bpe.model`; see `config/models/asr/sherpa_file_asr.conf`. The configured mix (INT8 encoder/joiner, FP32 decoder) matches upstream's documented file example. Local test WAVs 0/1/2/3/4/46 are PCM mono S16 16 kHz. The model and WAVs remain outside the product tree. |
| Model source | Sherpa upstream [model catalog](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16-bilingual-chinese-english) links a release archive and describes derivation from [csukuangfj/k2fsa-zipformer-bilingual-zh-en-t](https://huggingface.co/csukuangfj/k2fsa-zipformer-bilingual-zh-en-t). Matching local file sizes support identification; archive-level hash/provenance was not supplied. |
| Model license | The linked Hugging Face source model card labels Apache-2.0, but the local extracted ONNX package has no license/notice and its conversion and bundled test WAV redistribution terms have not been independently established. `LICENSE_UNVERIFIED_FOR_DISTRIBUTION`; local research only, no product packaging. Dataset license is a separate unknown. |
| Known limits | File-only, 16 kHz mono S16 PCM, no resampling, no live device, no VAD/wake, no board performance claim. Sherpa C API calls are serialized by one backend instance. Runtime model-hash verification is not implemented. |

Configured local research files, with hashes recalculated from the local reference tree:

| File | Bytes | SHA256 |
| --- | ---: | --- |
| `encoder-epoch-99-avg-1.int8.onnx` | 42,980,793 | `db6f51551762e40e549166fe041ea3e45464370b595e9ad23f06478ec3794fbb` |
| `decoder-epoch-99-avg-1.onnx` | 13,877,276 | `89be509a83175261695bdef5fd1c7b9ab1129a663d1284e7ba9f8507b21e0906` |
| `joiner-epoch-99-avg-1.int8.onnx` | 3,228,485 | `bdda356d6f9b8c2d7cee9ee0e26075fa537490f7fd06520be408d287073667b9` |
| `tokens.txt` | 56,317 | `a8e0e4ec53810e433789b54a5c0134a7eaa2ffca595a6334d54c00da858841d3` |
| `bpe.model` | 244,836 | `bcae393dbc5611be5ffa4c7ae0841558978a5a4f484008cb9dff3a2cc97ebe01` |

The five configured assets total 60,387,707 bytes. `bpe.model` is inventoried but the selected greedy C API path does not open it explicitly. The six bundled WAV fixtures have no license notice beyond `test_wavs/readme.md` saying `test data`; their use here is local verification only. `test_wavs/0.wav` SHA256 is `7d93384ca14702cc584a7a33fe2fed92e89e708549161cb12ea38c916882103b`.

Validation on this Host: default CI 9/9 CTest and 6/6 Python; real Sherpa ON configure/build passed; optional `ctest -L sherpa-integration` 2/2 passed (known WAV text and old-session cancellation/new-session recognition). The WAV reader accepted `0.wav`. Actual `ASR_FINAL` was `昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`, matching the upstream documented phrase. Two WAVs decoded sequentially after one `MODEL_LOADED`; first decode 0.237 s / 10.053 s audio, second 0.121 s / 5.100 s audio (Host only). No ASR model file or test WAV was copied into the main repository.

Sherpa's upstream code and the `LLM_Voice_Flow` author's `voice/sherpa-onnx/sherpa-onnx/voice/` microphone integration have different provenance. This task consumes neither source tree in the main Git repository.
