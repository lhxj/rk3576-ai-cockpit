# RTSP backend inventory

Date: 2026-10-02.

## Host

Read-only inventory found `libgstrtsp-1.0.so.0`, which is the GStreamer RTSP
protocol library, but did not find the `gstreamer-rtsp-server-1.0` pkg-config
module, live555 development files, or an existing project RTSP server. No
package was installed.

Decision: implement the project-specific minimum server in
`apps/media_srv/src/rtsp_server.cpp`. It supports one UDP-unicast H.264 client
and only OPTIONS/DESCRIBE/SETUP/PLAY/TEARDOWN. It is not presented as a reusable
general RTSP stack.

## Board

The four bounded read-only T0 attempts used:

```text
ssh -o BatchMode=yes -o ConnectTimeout=... lubancat '<identity, wlan0, ss,
client-tool, package and MPP inventory>'
```

All failed before login with `connect to host 10.34.122.223 port 22:
Connection timed out`. Therefore installed board RTSP/client packages, the
current `wlan0` IPv4 address, and port 8554 occupancy are still unobserved in
this branch. No board package or configuration was changed.
