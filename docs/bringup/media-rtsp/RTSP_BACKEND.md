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

The four bounded read-only T0 attempts to the former `10.34.122.223` address
timed out and remain historical evidence. With the user-provided current
address, `ssh -o HostName=10.232.249.223 lubancat` succeeded while preserving
the alias identity. No global SSH setting was changed.

The board already contained ffprobe/ffplay 5.1.8, GStreamer and mpv. It did not
need a new RTSP library or package installation. Port 8554 was initially free.
The minimum project server built natively against the installed MPP stack and
the Qt5 real-CAM0 target also linked. Tests bind to the configurable default
`0.0.0.0:8554` and use `/cam0`; all final shutdown checks found the port free.
