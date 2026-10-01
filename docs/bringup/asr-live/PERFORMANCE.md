# Live ASR resource baseline

T0: peak observed RSS 1388 kB, PSS 4861 kB (short-process sampling can race exit), minimum MemAvailable 3069728 kB, peak thermal zone reading 48076 m°C.

T1: 32000 captured frames in 2.014 s, XRUN 0; peak observed RSS 11104 kB, PSS 5983 kB, minimum MemAvailable 3068268 kB, peak reported CPU 66.6%, peak thermal zone 48076 m°C. PSS and RSS are approximate short-interval `/proc` samples. No model was loaded for T0/T1.

| Gate | Load ms | Frames | First partial ms | Capture stop to FINAL ms | Queue max / cap | XRUN / overflow | Peak RSS / PSS kB | Lowest MemAvailable kB | Peak CPU / temp |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| T2 spoken | 7539.13 | 80000 | 1185.59 | 0.52 | 5 / 100 | 0 / 0 | 174732 / 171901 | 2910104 | 133% / 50.845°C |
| T3 cancel | 8214.35 | 31680 | none | n/a | 4 / 100 | 0 / 0 | 195264 / 192209 | 2889920 | 133% / 52.692°C |
| T4 cancel + restart | 7680.69 | 32000 + 80000 | 1792.03 on new session | 0.65 on new session | 6 / 100 max | 0 / 0 | 195272 / 192345 | 2892812 | 105% / 51.768°C |
| T5 three sessions | 7984.12 once | 80000 each | 831.49 / 1465.95 / 1143.43 | 0.79 / 0.44 / 0.83 | 5 / 100 max | 0 / 0 | 195332 / 192289 | 2890092 | 133% / 52.692°C |

T5 end-of-session PSS readings were 169415, 172199 and 172327 kB; short-run growth of about 3 MB occurred, with only 128 kB additional from session 2 to 3. This is not a long-duration leak test. CPU is the board process percentage reported by `ps` and can exceed 100% across cores. Temperatures are the highest sampled thermal-zone values without a governor or affinity change. No live RTF is claimed from the earlier file-only benchmark.
