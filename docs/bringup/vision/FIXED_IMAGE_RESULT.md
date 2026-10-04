# Fixed-image RKNN result

Command environment: LubanCat-3 v2, 2026-10-02, existing board utility and assets.

```text
/usr/bin/rknn_common_test \
  /usr/share/model/RK3576/mobilenet_v1.rknn \
  /usr/share/model/cat_224x224.jpg 1
```

Exit code was zero. The utility reported runtime 2.3.0, driver 0.9.8, one
224x224x3 NHWC input, one 1001-element output, 4.47 ms inference (223.46 FPS for
this one fixed-input measurement), and a non-empty Top-5 headed by class 283 at
0.415283. This proves the selected prebuilt artifact executes on this board. It is
not the live CAM0 rate.
