# CAM0 plus RKNN stability

T3 ran `vision_rknn_probe --mode vision-only` for 300 seconds on 2026-10-02.
It exited zero and released CAM0 and the RKNN context.

| Metric | Result |
|---|---:|
| capture frames | 8,963 |
| vision input frames | 8,962 |
| inferred frames | 2,241 |
| vision rate | 7.46906 fps |
| sampling drops | 6,721 (expected 30-to-8 fps throttling) |
| queue / stale drops | 0 / 0 |
| queue peak / capacity | 1 / 2 |
| preprocess / inference / end-to-end avg | 15.0968 / 2.96735 / 21.1242 ms |
| V4L2 sequence gap / DQBUF / QBUF | 0 / 0 / 0 |
| capture stream epoch | 1 |

Thirty ten-second resource samples reported CPU average 23.61% (one startup sample
reached 100%), process `ps` RSS peak 24,608 KiB, `smaps_rollup` PSS peak 33,738 KiB,
minimum MemAvailable 3,037,028 KiB and maximum sampled thermal-zone temperature
54.538 C. The unprivileged run could not read a numeric debugfs NPU-load value, so
no NPU utilization percentage is claimed.

T4 then ran Preview plus Vision for 60 seconds. It retained capture epoch one,
delivered 1,792 preview frames and 448 inference results at 7.47037 fps. Queue peak
was one; queue/stale drops and all V4L2 errors were zero. No encoder was created.
