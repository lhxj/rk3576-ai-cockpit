# RKNN environment inventory

Observed read-only on LubanCat-3 v2 at `10.232.249.223` on 2026-10-02.

| Item | Evidence |
|---|---|
| Board | RK3576, Debian 12, AArch64, kernel `6.1.99-rk3576` |
| Runtime | `/usr/lib/librknnrt.so`, `2.3.0 (c949ad889d@2024-11-07T11:35:33)` |
| Driver | `/sys/module/rknpu/version` = `0.9.8` |
| Device | platform `27700000.npu`, compatible `rockchip,rk3576-rknpu`; `/dev/dri/renderD129` |
| Model | `/usr/share/model/RK3576/mobilenet_v1.rknn`, 4,679,017 bytes |
| Model SHA256 | `bc66943ea85ec0dd8a04da22c4276bfc8a4c6fe24f5ea8be7a1e5c3c22c8259d` |
| Model input | NHWC, 1x224x224x3, INT8 internal, embedded mean/std 128/128 |
| Model output | `MobilenetV1/Predictions/Reshape_1`, 1x1001, FP16 internal |
| SDK header | official v2.3.0 `rknn_api.h`, local SHA256 `340f16c14bc86d41bc5a64251bd1e747dd50899a4438d08707b48d8de384165a` |

The model is byte-identical to the file at
`examples/functions/rknn_mobilenet_demo/model/RK3576/mobilenet_v1.rknn` in the
official `airockchip/rknn-toolkit2` v2.3.0 tag (Git blob
`db8a291dde3460c3330f5d50b7cad5550997d975`). The checked header is Git blob
`c50673...` from that same tag. Access date: 2026-10-02.

Sources:

- <https://github.com/airockchip/rknn-toolkit2/tree/v2.3.0>
- <https://github.com/airockchip/rknn-toolkit2/releases/tag/v2.3.0>
- <https://github.com/airockchip/rknn_model_zoo>

The toolkit repository LICENSE and header use Rockchip proprietary/all-rights-
reserved terms. The installed artifact and SDK header remain external to product Git.
`rknn_model_zoo` being Apache-2.0 does not relicense this toolkit sample model.
Local board use is validated here; redistribution remains restricted and requires a
separate Rockchip rights review.

The board had no installed `rknn_api.h`, so the build used the matching external
official v2.3.0 header and the system runtime. No package was installed and no model
was copied into this repository.
