# RTP validation

Date: 2026-10-02. Evidence level: HOST_TESTED + BOARD_TESTED_BOUNDED.

`media_rtsp_test` verifies:

- a small NAL is carried in one RTP packet;
- a large NAL uses FU-A with correct start/end bits;
- sequence numbers increment for every RTP packet;
- all fragments of an access unit use one 90 kHz timestamp;
- only the final access-unit packet carries the marker bit;
- SPS/PPS are cached and emitted before a joining client's IDR;
- P frames are withheld while a client waits for IDR;
- a bounded slow-network queue counts drops and resumes at a later IDR;
- local TCP RTSP OPTIONS/DESCRIBE/SETUP/PLAY/TEARDOWN succeeds;
- configured paths are enforced and TCP-interleaved SETUP is rejected;
- SDP contains `H264/90000`, the encoded rate and `sprop-parameter-sets`;
- two sequential sessions are accepted and reconnect is counted;
- stopping with an idle connected client wakes the control thread and closes
  the sockets cleanly.

Commands:

```text
cmake --build build/rtsp-host -j2 --target media_rtsp_test
ctest --test-dir build/rtsp-host --output-on-failure -R media_rtsp
```

Result: 1/1 PASS. The same test plus recording ran under ASan/UBSan: 2/2 PASS.

On RK3576, board ffprobe/ffmpeg recognized and consumed MPP output as H.264
High, yuv420p, 1632x1224, 30 fps and 90 kHz timebase. Two sequential sessions
decoded, with one reconnect and a 38 ms final join-to-first-IDR value. The
five-minute Preview+RTSP run delivered 256,323 RTP packets with zero application
queue drops. WSL received SDP over the board's wlan0 address and asserted
H264/90000, SPS/PPS, 1632x1224 and `a=framerate:30`.

Actual decode was performed by the board's existing clients. WSL had no media
decoder installed, so its evidence is off-board RTSP control reachability and
SDP validation rather than off-board RTP decode.
