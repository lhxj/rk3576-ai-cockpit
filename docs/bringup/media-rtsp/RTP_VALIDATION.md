# RTP validation

Date: 2026-10-02. Evidence level: HOST_TESTED.

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
- SDP contains `H264/90000` and `sprop-parameter-sets`;
- two sequential sessions are accepted and reconnect is counted.
- stopping with an idle connected client wakes the control thread and closes
  the sockets cleanly.

Commands:

```text
cmake --build build/rtsp-host -j2 --target media_rtsp_test
ctest --test-dir build/rtsp-host --output-on-failure -R media_rtsp
```

Result: 1/1 PASS. The same test plus recording ran under ASan/UBSan: 2/2 PASS.
This proves packet construction and local control flow, not RK3576 MPP output
or playback by an external media client.
