# RK3576 file ASR target environment

Observed by read-only SSH on 2026-10-01. Target: LubanCat-3 v2, `lubancat`, user `cat`.

| Item | Observation |
| --- | --- |
| Architecture | `aarch64` (`uname -m`) |
| Kernel | `6.1.99-rk3576`, build `#8` dated 2026-04-24 |
| Distribution | Debian GNU/Linux 12 (bookworm) |
| GCC / G++ | Debian 12.2.0 |
| CMake / Make | 3.25.1 / 4.3 |
| glibc | 2.36 (`ldd`, `getconf`) |
| Memory | 3,988,492 kB total; 3,014,244 kB MemAvailable at inventory; no swap |
| Storage | `/home/cat` on `/dev/mmcblk0p3`, 29G total, 22G available |
| Thermal | Six readings: 45.307, 46.230, 45.307, 44.384, 46.230, 46.230 °C (millidegree sysfs values divided by 1000) |
| System libraries | `/lib/aarch64-linux-gnu/libstdc++.so.6` and `libgcc_s.so.1` present; no cached Sherpa/ONNX library detected by `/sbin/ldconfig -p` |
| Deployment tooling | SSH, SCP, `file`, `readelf`, `sha256sum` available. `rsync` and `/usr/bin/time` absent from `cat`'s command path. |
| Occupancy | No running `cockpit_asr_file_test` or `sherpa_file_cancel_test` observed before deployment. `/home/cat/cockpit` was writable by `cat`. |

No environment variables, SSH config, credentials, device nodes or system settings were read or changed. This inventory is a point-in-time resource snapshot; the performance report records measurements during file ASR separately.
