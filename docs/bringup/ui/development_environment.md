# Cockpit UI development environment installation

Date: 2026-10-01

Scope: distro-provided Qt5 development packages only. No Qt upgrade, third-party
installer, replacement system library, display-manager change, or boot change.

## WSL Host

Environment: Ubuntu 22.04.5, x86_64.

Installed through the Ubuntu package source:

```text
qt5-qmake:amd64        5.15.3+dfsg-2ubuntu0.2
qtbase5-dev:amd64      5.15.3+dfsg-2ubuntu0.2
qtbase5-dev-tools      5.15.3+dfsg-2ubuntu0.2
```

The normal WSL user required an interactive sudo password. Installation used
the WSL host's supported `-u root` launch for this distro; sudo policy and user
permissions were not modified.

Post-install validation:

- `qmake --version`: Qt 5.15.3;
- `pkg-config --modversion Qt5Core Qt5Widgets`: both 5.15.3;
- `bash scripts/dev/host_ci.sh`: exit 0;
- 4/4 CTest passed, including `cockpit_ui_offscreen_startup`;
- 6/6 Python repository tests passed;
- resulting UI binary is x86-64 ELF.

## LubanCat target

Environment: Debian 12 arm64, existing Qt runtime 5.15.8. Before installation,
the root filesystem had about 22 GB free. Passwordless non-interactive sudo was
available for the existing `cat` user.

Installed through the configured Debian Bookworm mirror:

```text
qt5-qmake:arm64        5.15.8+dfsg-11+deb12u3
qtbase5-dev:arm64      5.15.8+dfsg-11+deb12u3
qtbase5-dev-tools      5.15.8+dfsg-11+deb12u3
```

The package transaction downloaded about 4.7 MB and added about 48.7 MB. It did
not upgrade or remove packages.

Source was copied only to
`/home/cat/cockpit/ui-foundation-20261001-qtdev/`. Native configure and build
used GCC 12.2.0 and Qt 5.15.8. Results:

- 4/4 target CTest passed, including the offscreen Qt startup test;
- generated binary is a dynamically linked ARM aarch64 ELF;
- binary SHA256:
  `ba09a0abef480bbf5484e2ae36ace441defe9ff19532fb4b4d40b7652c9c9fcb`.

The active GNOME Shell environment was read from its process environment instead
of guessed: `DISPLAY=:0`, `XAUTHORITY=/run/user/1000/gdm/Xauthority`, and
`XDG_RUNTIME_DIR=/run/user/1000`. A two-second full-screen X11 launch with the
Mock backend exited 0 when using `QT_QPA_PLATFORM=xcb` and
`QT_XCB_GL_INTEGRATION=none`.

Without the latter variable, the process still exited 0 but Mesa printed Rockchip
DRI2/DRI3 driver-load errors. The current Widgets UI does not require OpenGL, so
disabling XCB GL integration is the minimal launch workaround; this is not
evidence that the GL acceleration path is healthy.

No screenshot tool was available and no new one was installed. Window geometry,
text readability, clipping, and touch behavior therefore still require human
confirmation. No Camera, Audio, AMP, sensor, NPU, or model backend was started.
