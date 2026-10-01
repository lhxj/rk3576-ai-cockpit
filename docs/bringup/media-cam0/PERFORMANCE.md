# CAM0 preview performance

Status: `BOARD_TESTED_T8_FIVE_MINUTE_PASS`.

Date: 2026-10-02. The 20-second real Qt preview run measured:

| metric | observed |
|---|---:|
| V4L2 capture | 29.8767 fps, 583 frames |
| Qt delivered preview | 13.0096 fps, 253 frames |
| capture sequence gaps | 0 |
| DQBUF / QBUF errors | 0 / 0 |
| mailbox / GUI coalescing drops | 0 / 0 |
| intentional throttle skips | 323 |
| process RSS at 5 s (`ps`) | 69,648 KiB |
| process CPU at 5 s (`ps`) | 80.0% |
| threads at 5 s | 16 |

A later read during the T7 interactive run showed `smaps_rollup` RSS 82,832 KiB and
PSS 70,489 KiB, process CPU 79.6%, SoC 51.769 C, big core 53.615 C, little core
52.692 C, DDR 51.769 C, NPU 51.768 C and GPU 53.615 C. These are point samples, not
peaks or thermal qualification. The software NV12 conversion deliberately targets
about 10-15 displayed fps; RGA/DMA-BUF optimization remains deferred.

## T8 bounded five-minute run

The same real profile ran for 300 seconds with Camera page active and exited through
the Qt timer. It captured 8,960 frames at 29.8757 fps with zero sequence gaps,
poll timeouts and DQBUF/QBUF errors. Qt delivered 4,023 updates at 13.4174 fps;
mailbox and GUI-coalescing drops were zero and 4,935 frames were intentionally
skipped by the preview throttle. After exit there was no `cockpit_ui` process or
Camera owner, and a new T1 negotiate/open/close completed successfully.

The RSS/PSS, CPU and thermal values above were sampled during this five-minute process.
This passes the requested short bounded stability gate; it is not long-term stability
or thermal qualification. T7 physical touch/visual acceptance was later completed
and is recorded separately in `UI_PREVIEW_RESULT.md`.
