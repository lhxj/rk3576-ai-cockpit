// SPDX-License-Identifier: GPL-2.0
/* Candidate Linux peer for the M0 HELLO/PING smoke test. Host source only. */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/string.h>

#define AMP_ECHO_SERVICE "rk3576-m0-echo"

static int amp_echo_probe(struct rpmsg_device *rpdev)
{
	static char hello[] = "HELLO";
	dev_info(&rpdev->dev, "M0 echo channel src=%u dst=%u\n",
		 rpdev->src, rpdev->dst);
	return rpmsg_send(rpdev->ept, hello, sizeof(hello) - 1);
}

static int amp_echo_callback(struct rpmsg_device *rpdev, void *data, int len,
			     void *priv, u32 src)
{
	static char ping[] = "PING";
	(void)priv;
	(void)src;
	if (len == 9 && !memcmp(data, "HELLO_ACK", 9)) {
		dev_info(&rpdev->dev, "HELLO_ACK\n");
		return rpmsg_send(rpdev->ept, ping, sizeof(ping) - 1);
	}
	if (len == 4 && !memcmp(data, "PONG", 4)) {
		dev_info(&rpdev->dev, "PONG\n");
		return 0;
	}
	dev_warn(&rpdev->dev, "unexpected M0 reply: %d bytes\n", len);
	return -EINVAL;
}

static void amp_echo_remove(struct rpmsg_device *rpdev)
{
	dev_info(&rpdev->dev, "M0 echo channel removed\n");
}

static const struct rpmsg_device_id amp_echo_id_table[] = {
	{ .name = AMP_ECHO_SERVICE },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, amp_echo_id_table);

static struct rpmsg_driver amp_echo_driver = {
	.drv.name = "rk3576_amp_echo_test",
	.id_table = amp_echo_id_table,
	.probe = amp_echo_probe,
	.callback = amp_echo_callback,
	.remove = amp_echo_remove,
};
module_rpmsg_driver(amp_echo_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RK3576 M0 RPMsg HELLO/PING candidate test");
