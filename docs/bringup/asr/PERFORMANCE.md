# RK3576 file ASR resource baseline

Measured on LubanCat-3 v2 / RK3576 / Debian 12 / AArch64, stock frequency policy, no audio device. This is a short file-input baseline, not a sustained or combined workload test. `0.wav` duration is 10.0531 s. RTF = decode elapsed / audio duration; model load is reported separately.

| Phase | Model load | Decode / RTF | Peak RSS | Peak PSS | Lowest sampled MemAvailable | Peak sampled CPU | Peak sampled thermal |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Load only | 7,617.91 ms | — | 197,492 kB (`ru_maxrss`; sampler 194,260) | 189,145 kB | 2,894,244 kB | 150% | 53.615 °C |
| One 0.wav | 7,871 ms | 1.73939 s / 0.17302 | 205,436 kB (`ru_maxrss`; sampler 197,816) | 195,017 kB | 2,888,228 kB | 105% | 55.461 °C |
| Three sequential 0.wav in one process | 7,707.7 ms once | 1.70992 / 1.70431 / 1.70699 s; RTF 0.170089 / 0.16953 / 0.169797 | 194,916 kB | 192,321 kB | 2,890,616 kB | 133% | 56.384 °C |
| Real engine cancel then fresh session | Included in test | Not timed separately | sampler 190,464 kB | 188,077 kB | 2,889,604 kB | 133% | 59.153 °C |

The file tool reads `/proc/self/status`, `/proc/self/smaps_rollup`, `getrusage`, and `/proc/meminfo` before and after load and decode. The board test wrapper polls the child every 100 ms for RSS, PSS, MemAvailable, CPU%, and thermal-zone temperatures. Different instantaneous and high-water measurements need not match. The lowest MemAvailable is the lowest *sampled* value, not a hardware-guaranteed minimum. CPU percentages above 100 indicate multicore CPU time. Temperature is the maximum across thermal zones; no zone-to-SoC mapping or throttling state was verified. No governor, affinity, or clock setting was changed.

The previously measured x86 Host RTF was about 0.024. It is context only and was not used as a board acceptance threshold. The board result is CPU/ONNX Runtime inference; it is not RK3576 NPU performance. These figures do not establish safe simultaneous vision plus voice residency on a 4 GB device.
