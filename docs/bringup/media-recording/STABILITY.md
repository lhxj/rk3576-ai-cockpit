# Preview + Recording stability

## Final bounded run

The final command ran shared Preview + Recording for 300 seconds with an outer
390-second timeout and five-second resource sampling. Exit code was 0.

| Metric | Result |
|---|---:|
| Capture frames / fps | 8,974 / 29.8755 |
| Input / encoded frames | 8,964 / 8,964 |
| Encoded fps | 29.8778 |
| Output packets / bytes | 8,964 / 301,328,134 |
| Queue peak / overflow | 1 / 0 |
| Sequence gaps / poll timeouts | 0 / 0 |
| DQBUF / QBUF / encoder errors | 0 / 0 / 0 |
| File closed | true |
| Resource samples | 59 |
| CPU min / mean / max | 19.1% / 20.4% / 60.0% |
| RSS min / max | 15,100 / 23,848 KiB |
| PSS min / max | 14,043 / 22,843 KiB |
| Threads min / max | 5 / 5 |
| Thermal min / max across zones | 48.076 / 52.692 °C |

The first CPU sample covers process startup and is the observed 60.0% maximum;
the settled samples were about 20%. The file SHA256 was
`84a8f07ec215ed80320e97fd2d4b7d12e005bbf22e557c66f997ce9a19cf2cad`.
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
