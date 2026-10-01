# RK3576 target Qt and display inventory

Observed: 2026-10-01 (Asia/Shanghai)

Method: serialized read-only SSH through the existing `lubancat` alias

Raw evidence: Git-ignored `artifacts/local/ui-target-inventory-20261001.txt`

## Commands and results

Host checks were run in Ubuntu 22.04.5 WSL:

| Command | Exit | Result |
|---|---:|---|
| `qmake --version` | 1 | command not found |
| `qtpaths --version` | 1 | command not found |
| `pkg-config --modversion Qt5Core` | 1 | package not found |
| `pkg-config --modversion Qt6Core` | 1 | package not found |
| `aarch64-linux-gnu-g++ --version` | 0 | GCC 11.4.0 |

The target inventory used `BatchMode=yes`, `StrictHostKeyChecking=yes`, a
45-second timeout, and the repository board lock. The SSH command exited 0.
It queried only `qmake`, `qtpaths`, dpkg package names, login/session state,
process names, DRM/sysfs display state, and build-tool presence.

## Board observations

Evidence level: `BOARD_OBSERVED_READONLY`.

- Active user session: user `cat`, `Type=x11`, `Active=yes`, `State=active`,
  `Remote=no`.
- Desktop processes: Xorg and GNOME Shell are running.
- SSH shell environment: no `DISPLAY`; `XDG_SESSION_TYPE=tty`. A remote command
  therefore cannot safely assume the GUI display or Xauthority.
- HDMI-A-1 is connected and enabled.
- Framebuffer reports `U:800x480p-0` and virtual size `800,480`.
- The connector mode list includes 800x480 and several larger modes. The
  framebuffer observation is the evidence for the current 800x480 mode; the
  mode list alone would not prove it.
- Installed Qt runtime packages are arm64 Qt 5.15.8
  (`libqt5core5a`, `libqt5gui5`, `libqt5widgets5`, plus DBus, Network, OpenGL,
  and Test runtime libraries).
- `qmake` and `qtpaths` are absent. Qt pkg-config metadata was not found.
- `cmake`, `g++`, and `pkg-config` are present, but the absence of Qt development
  metadata means this inventory does not establish a native board UI build path.

## Consequence

The target Qt family is Qt 5, version 5.15.8, on GNOME/X11. The application
should prefer Qt5 on this image and remain source-compatible with Qt6 where that
does not add complexity. No system Qt upgrade is justified.

This inventory does not authorize setting `DISPLAY=:0`, does not prove remote
Xauthority access, and does not establish an AArch64 build. Board display and
touch validation remain pending until a verified sysroot or target Qt development
environment produces the executable and the user can perform the touch checks.
