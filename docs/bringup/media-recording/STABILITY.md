# Preview + Recording stability

## Final bounded run

The final command ran shared Preview + Recording for 300 seconds with an outer
390-second timeout and five-second resource sampling. Exit code was 0.

| Metric | Result |
|---|---:|
| Capture frames / fps | 8,976 / 29.8755 |
| Preview delivered frames / fps | 8,965 / 29.8775 |
| Preview mailbox replacements | 9 |
| Input / encoded frames | 8,965 / 8,965 |
| Encoded fps | 29.8782 |
| Output packets / bytes | 8,965 / 300,028,816 |
| Queue peak / overflow | 1 / 0 |
| Sequence gaps / poll timeouts | 0 / 0 |
| DQBUF / QBUF / encoder errors | 0 / 0 / 0 |
| File closed | true |
| Resource samples | 59 |
| Steady CPU min / mean / max | 18.3% / 18.9% / 22.9% |
| Steady RSS | 20,944 KiB |
| Steady PSS | 19,927 KiB |
| Steady threads | 6 |
| Steady thermal min / max across zones | 45.307 / 51.768 °C |

The first sample crossed the `stdbuf` exec transition and is excluded from the
58-sample steady memory/thread summary; its lifetime-average CPU observation was
60.0%. The file SHA256 was
`0f4333ea51299eaee70480b2cecb3290d021a5eab09b86d2b38c259adb3e52f1`.
The first 16 MiB contained nine SPS, nine PPS, nine IDR and 497 slice NALs.
`ffprobe` identified H.264 High profile level 4.0, 1632x1224, yuv420p, 30/1 fps.
CAM0 had no owner after exit.

## Corrected validation-tool failure

The first 300-second attempt reached a 301,543,910-byte output but the outer
330-second watchdog returned 124. The encoder had completed the requested data
volume; the probe then loaded and scanned the entire 300 MB file and exceeded
the remaining validation window. Its resource sampler also followed only a
child PID and recorded zero samples when `timeout` executed the target directly.

The probe was corrected to scan a bounded first 16 MiB without overlapping
three/four-byte start-code counts. The sampler now follows the timeout child or
falls back to the timeout PID, uses line-buffered output, and has an exact-PID
cleanup trap. The complete 300-second run above then exited 0. The interrupted
file is not used as PASS evidence.

This is a bounded five-minute engineering run, not a long-term storage,
thermal, power-loss or media-container qualification.
