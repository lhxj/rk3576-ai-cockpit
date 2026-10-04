# Preview, recording, RTSP and vision concurrency

T5 ran one `MediaService` for 90 seconds with Preview, Annex-B recording, the
single-client RTSP service and RKNN Vision active together.

Observed project metrics:

- CAM0 capture frames 2,693, stream epoch one, no sequence gaps or DQBUF/QBUF errors;
- Preview deliveries 2,691;
- RKNN results 673 at 7.46995 fps, queue peak one, queue/stale drops zero;
- MPP encoded 2,692 frames with exactly one encoder start;
- recording queue overflow zero;
- RTSP sent 7,855 RTP packets with zero packet drop;
- output file 89,700,250 bytes, SHA256
  `b019b3eaaf34b725873ab1d3fd426121e3e180ae87991a7d63f3b8e721805bdc`.

The existing board `ffprobe` client reported H.264, 1632x1224 and 30/1 fps from
`rtsp://127.0.0.1:8556/cam0`. A subsequent existing `ffmpeg` client decoded 241
frames over eight seconds. The closed recording reported the same codec, dimensions
and rate. RTSP port 8556 and the test process were absent after shutdown.

Nine ten-second resource samples reported CPU average 56.73% (startup peak 200%),
`ps` RSS peak 38,124 KiB, PSS peak 35,703 KiB, minimum MemAvailable 2,937,412 KiB
and maximum temperature 53.615 C. This is a 90-second combination test, not a
long-duration thermal certification.

This scenario establishes one CAM0 capture and one MPP encoder. Vision consumes the
same owned frames before encoder submission and does not open a second V4L2 node.
