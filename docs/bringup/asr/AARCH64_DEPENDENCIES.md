# AArch64 Sherpa dependency closure

The board path uses the official [Sherpa-ONNX v1.11.3 release](https://github.com/k2-fsa/sherpa-onnx/releases/tag/v1.11.3), asset `sherpa-onnx-v1.11.3-linux-aarch64-shared-cpu.tar.bz2` (19,003,169 bytes). Downloaded with `gh release download` into ignored local `build/asr-deps`; archive SHA256 `8dc62c081c90d30b15e0912c98863117ec61bd8b1729a3019a6ec52826a90733`. The release API did not provide a separate upstream checksum; this digest is our acquired-file identity, not independently signed provenance.

Only two shared objects were extracted/deployed. No packaged microphone, ALSA, TTS, or other example executable was run. The AArch64 archive contains no C API headers. The architecture-independent `c-api.h` came from the official v1.11.3 x64 shared release already used for Host validation, SHA256 `92e1b79aecb77bc39dcd89fd830c5a5fbbddf0b20d8397a7331f09077052a2a5`; no x64 binary was deployed. The main project has no Sherpa source/vendor copy.

| File | SHA256 | ELF / SONAME | Direct NEEDED | Maximum version needs |
| --- | --- | --- | --- | --- |
| `libsherpa-onnx-c-api.so` | `d1c311595cf5091eac1660b18b065ca5f180b852e8fb0ed9e82c6867cf1aea89` | ELF64 AArch64; same SONAME | `libonnxruntime.so`, `libpthread.so.0`, `libm.so.6`, `libstdc++.so.6`, `libgcc_s.so.1`, `libc.so.6`, AArch64 loader | ONNX Runtime `VERS_1.17.1`; `GLIBC_2.17`; `GLIBCXX_3.4.18`; `CXXABI_1.3.5` |
| `libonnxruntime.so` | `89abc6da8d7ac84e5270349fe8a44ee7469d9a958797eed004f1a3f5c7df21d4` | ELF64 AArch64; SONAME `libonnxruntime.so` with version definition `1.17.1` | `libdl.so.2`, `librt.so.1`, `libpthread.so.0`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, AArch64 loader | `GLIBC_2.17`; `GLIBCXX_3.4.19`; `CXXABI_1.3.7` |

`file`, `readelf -h`, `readelf -d`, and `readelf --version-info` were run locally on the exact AArch64 objects before deployment. Target has glibc 2.36 and GCC 12.2 `libstdc++`, so static version requirements fit; actual loader resolution and executable startup are validated separately on the board. Both libraries carry `$ORIGIN` RPATH. We use `LD_LIBRARY_PATH=/home/cat/cockpit/asr-target/lib` only for the test process, never system library replacement.

Sherpa upstream source license is Apache-2.0; ONNX Runtime upstream source license is MIT. This build does not establish distribution clearance for the release bundle, model conversion package, or bundled WAV. Model and WAV remain `LICENSE_UNVERIFIED_FOR_DISTRIBUTION` and are used for local research validation only.
