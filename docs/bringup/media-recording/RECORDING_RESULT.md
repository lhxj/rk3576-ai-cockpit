# CAM0 recording result

All tests ran serially as user `cat`. Immediately before the real tests the
media graph resolved OV8858 `m00_b_ov8858 3-0036` through `/dev/media1`
`rkisp_mainpath` to `/dev/video11`; `/dev/video-camera0` also resolved to that
node for this boot. The node was still passed explicitly and was not treated as
a permanent physical identity.

## T2: ten-second real recording

```sh
timeout -k 5s 35s build/apps/media_srv/media_recording_probe \
  --mode record-only --device /dev/video11 --seconds 10 \
  --output-dir artifacts/recording-t2
```

Exit code was 0. Results:

| Metric | Value |
|---|---:|
| Capture frames / fps | 301 / 29.8772 |
| Input / encoded frames | 300 / 300 |
| Encoded fps | 29.9766 |
| File bytes | 9,757,751 |
| Queue peak / overflow | 1 / 0 |
| Sequence gaps / poll timeouts | 0 / 0 |
| DQBUF / QBUF / encoder errors | 0 / 0 / 0 |
| File closed | true |

Path was `artifacts/recording-t2/recording_1.h264`; SHA256 was
`5469d757a17630c6759f460bdacf959fb42735998fd54387762f474ac8d7c79b`.
The scanner found SPS, PPS, IDR and slice NALs. `ffprobe` identified H.264 High
profile level 4.0, 1632x1224, yuv420p and 30/1 fps. Duration was the requested
10 seconds; the stream contains 300 encoded frames.

## T3: twenty start/stop cycles

All 20 cycles completed with ten input and ten encoded frames per cycle. Every
file contained SPS, PPS, IDR and slice NALs. Sizes ranged from 231,105 to
264,196 bytes and the complete per-file SHA256/size manifest is retained in the
ignored local evidence and board deployment. Epochs increased from 1 through 20,
there was no EBUSY, and CAM0 reopened each cycle.

Cold FD count was 8. The installed MPP runtime retained one process-level device
FD after its first initialization: first-cycle and final counts were both 9.
Thus cycles 1 through 20 showed no cumulative FD growth, but the one process-wide
MPP FD is explicitly disclosed. Thread count was 1 before and after the service.

## T4/T5 ownership modes

- Preview + Recording for 10 seconds: 344 capture frames, 300/300 recording
  frames, 29.8787 capture fps, 29.9804 encoded fps, queue peak 1, no errors.
  Stopping Recording left Preview capture active; stopping Preview then released
  CAM0. The output was 9,803,323 bytes with SHA256
  `68ba7db92aaf3f05366f5c957aec0e49b64bc423f4190b670878b48568483c82`.
- Recording-only for 10 seconds: Preview remained Stopped; CAM0 was started only
  for the recorder and released after STOP. It encoded 300/300 frames at
  29.9560 fps with queue peak 1 and no errors. The output was 9,793,458 bytes
  with SHA256
  `530480baaca808ffd264e66a5c797a1627130f8f7a56212079626a557c086348`.

## T6 synthetic ASR_FINAL

Synthetic FINAL `开始录像` and `停止录像` passed through the deterministic
Intent router, Vehicle Core, `RealMediaServiceAdapter`, the same MediaService
and MPP recorder. START ACK lifecycle 9 preceded RESULT 10; canonical Recording
became `RECORDING` with source `RUNTIME`. STOP ACK 11 preceded RESULT 12; the
file was closed and Preview remained Streaming. Duplicate START FINAL reached
MediaService once. The one-frame proof file was 84,450 bytes with SHA256
`eeda287a188a4a2ed4b5ece14712fc8ab4e2fab4c59b61edce2c463cdf326935`.
Rear and RTSP remained `UNAVAILABLE`. Final CAM0 reopen passed.

The generated H.264 files remain under the board's user-owned deployment and are
not tracked by Git.
