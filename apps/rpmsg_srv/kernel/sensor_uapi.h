/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef RK3576_SENSOR_UAPI_H
#define RK3576_SENSOR_UAPI_H
#include <linux/types.h>
#include <linux/ioctl.h>
#define SENSOR_IOC_STATE _IOR('S', 1, struct sensor_link_state)
#define SENSOR_ABI 1
struct sensor_link_state {
 __u32 abi, owner_ready, removed, reserved;
 __u64 generation, sample_overwrites, control_drops, malformed, send_failures;
};
/* read: exactly one complete <=256-byte wire packet; short buffer => EMSGSIZE.
 * write: exactly one packet, nonblocking trysend; EAGAIN has no retry inside KO.
 * single open; mode0600; close discards queued packets. No automatic wire traffic.
 */
#endif
