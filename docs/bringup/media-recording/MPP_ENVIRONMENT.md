# RK3576 MPP environment

Observed read-only on LubanCat-3 v2 on 2026-10-02 before opening CAM0.

## Installed delivery

| Item | Observed value |
|---|---|
| OS / architecture | Debian 12 / AArch64 |
| Kernel | `6.1.99-rk3576` |
| Runtime package | `librockchip-mpp1 1.5.0-1 arm64` |
| Development package | `librockchip-mpp-dev 1.5.0-1 arm64` |
| Samples | `rockchip-mpp-demos 1.5.0-1 arm64` |
| V4L wrapper | `libv4l-rkmpp 1.7.0-1 arm64` |
| Runtime self-report | commit `43a191ed`, 2025-03-26 |
| pkg-config version | `rockchip_mpp 1.3.9` |
| SONAME | `librockchip_mpp.so.1` |
| Runtime SHA256 | `1aca0bed4ba184f5fef4841e381e8b9918983df02ebfd8c3983c6919acdc8bc5` |

The package version, pkg-config version and runtime commit label disagree. They
are recorded separately and were closed by a native compile plus actual encoder
tests; they must not be normalized to one invented version.

`ldd /usr/lib/aarch64-linux-gnu/librockchip_mpp.so.0` resolved AArch64 glibc,
`libstdc++.so.6`, `libm.so.6`, `libc.so.6` and `libgcc_s.so.1`. Debian package
metadata requires `libc6 >= 2.34` and `libstdc++6 >= 5`; the development package
requires the exact `librockchip-mpp1 1.5.0-1` runtime. Headers include
`rk_mpi.h`, MPP buffer/frame/packet APIs and encoder configuration commands.

`mpp_info_test` exited 0 and reported H.264 encoder support through the installed
delivery. The runtime library is Rockchip MPP; the installed Debian copyright
file identifies upstream Apache-2.0 code while Debian packaging has its own
GPL-2+ terms. Product redistribution still needs the package notices preserved.

No package was installed, upgraded or replaced for this work.
