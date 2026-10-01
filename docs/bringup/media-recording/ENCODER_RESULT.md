# MPP encoder result

## Native build

The project was configured and compiled on the RK3576 board with GCC 12.2 and:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCOCKPIT_ENABLE_V4L2_CAMERA=ON -DCOCKPIT_ENABLE_MPP_RECORDING=ON
cmake --build build -j2
```

Exit code was 0. CMake resolved `rockchip_mpp` through pkg-config and reported
MPP recording ON. The first native compile exposed and fixed a const-only NAL
probe access error; the production recorder source had already compiled.

## T1 synthetic NV12

Command:

```sh
timeout -k 5s 30s build/apps/media_srv/media_recording_probe \
  --mode synthetic --output-dir artifacts/recording-t1
```

Final exit code was 0. Sixty 1632x1224 NV12 frames produced 60 packets and 4,821
bytes. Queue peak was 6; overflow and encoder errors were 0; the file was closed.
The corrected Annex-B scanner found one SPS, one PPS, one IDR and 59 non-IDR
slices. SHA256 is
`50b31e48bc1fc9bb1c1a6ea626d3ba4c7c6a454cb3f2f7f91c4532155e63843f`.
`ffprobe` identified H.264 High profile, level 4.0, 1632x1224, yuv420p.

This proves the installed MPP encoder path and selected configuration execute on
the RK3576. It does not by itself prove CAM0 capture or long-duration recording.
