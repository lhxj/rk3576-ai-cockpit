# Stream result

Status: `BOARD_TESTED_T2_T3_PASS`.

Date: 2026-10-02. The project backend, not `v4l2-ctl`, performed these tests as
user `cat` against the T0-resolved `/dev/video11`.

## T2 bounded capture

Command (exit 0):

```sh
timeout 30s build/apps/media_srv/media_cam0_probe \
  --mode capture --device /dev/video11 --frames 300
```

The waiter requested at least 300 callbacks; one frame already in flight was
completed before STREAMOFF, so backend statistics contain 301 frames. Elapsed time
was 10.041 s and measured capture rate was 29.8775 fps. Sequence gaps, poll
timeouts, DQBUF errors and QBUF errors were all zero. `bytesused` was exactly
2996352 for every observed frame.

## T3 restart

Command (exit 0):

```sh
timeout 60s build/apps/media_srv/media_cam0_probe \
  --mode restart --device /dev/video11 --cycles 20 --frames-per-cycle 10
```

All 20 cycles completed. `stream_epoch` increased strictly from 1 through 20.
Each cycle observed at least ten callbacks and no DQBUF/QBUF error. Process fd
count was 4 before and after; thread count was 1 before and after. There was no
hang, `EBUSY`, residual worker or mapped-buffer/fd growth in this bounded test.
