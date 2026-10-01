# CAM0 device resolution

Date: 2026-10-01

Evidence: `BOARD_OBSERVED_READONLY`, user `cat`, kernel `6.1.99-rk3576`. No stream was
started during this stage.

## Resolved path

- sensor: `m00_b_ov8858 3-0036`;
- sensor format: `SBGGR10_1X10`, 1632x1224, media graph interval 10000/300000;
- sensor/CIF media device: `/dev/media0`, driver `rkcif-mipi-lvds`;
- ISP media device: `/dev/media1`, model `rkisp0`, driver `rkisp-vir0`;
- capture entity: `rkisp_mainpath`;
- capture node: `/dev/video11`;
- current alias: `/dev/video-camera0 -> /dev/video11`;
- capture driver: `rkisp_v10` 2.9.0;
- graph: OV8858 -> `rockchip-csi2-dphy0` -> `rockchip-mipi-csi2` -> rkcif ->
  `rkisp-isp-subdev` -> `rkisp_mainpath`.

The alias and numeric node are recorded runtime facts, not product constants. The
CAM0-real CLI will receive the resolved node explicitly.

## Current format evidence

`v4l2-ctl -d /dev/video11 --get-fmt-video` reports MPLANE 1632x1224 NV12, one memory
plane, bytesperline 1632 and sizeimage 2996352. The node advertises MPLANE capture and
streaming. User `cat` is in group `video`, the node is readable/writable, and `fuser`
reported no owner.

`VIDIOC_G_PARM` returned `Inappropriate ioctl for device`. The project backend must
record that the driver does not expose time-per-frame through this ioctl; measured
capture fps and the sensor media-graph interval remain separate evidence.
