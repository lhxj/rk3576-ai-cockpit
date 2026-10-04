# CAM0 RKNN result

T2 ran the project `vision_rknn_probe` for ten seconds with `/dev/video11`, the
runtime-resolved physical CAM0 for this boot. It used V4L2 MPLANE NV12 1632x1224,
CPU preprocessing and the board-installed MobileNetV1 artifact.

Observed result:

- model load 22.7335 ms; queried runtime 2.3.0 and driver 0.9.8;
- 299 captured frames and 298 submitted frames;
- 75 current-epoch inferences, 7.46996 FPS;
- preprocessing 14.2588 ms average; RKNN 2.89666 ms average;
- end-to-end 20.1161 ms average;
- queue peak one of capacity two; queue/stale drops zero;
- 223 expected sampling drops from 30 fps capture to the 8 fps target;
- V4L2 sequence gaps, DQBUF errors and QBUF errors all zero;
- capture stream epoch remained one; encoder instances zero.

The process stopped vision, unloaded RKNN, stopped CAM0 and exited zero with
`VISION_RKNN_CAM0_PROBE_PASS`.
