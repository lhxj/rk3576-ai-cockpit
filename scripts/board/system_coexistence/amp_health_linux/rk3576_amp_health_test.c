// SPDX-License-Identifier: GPL-2.0
/* Bounded coexistence health TEST. No new wire protocol or transport changes. */
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include "amp_health_state.h"

#define AMP_HEALTH_SERVICE "rk3576-m0-echo"

struct amp_health_peer {
    struct rpmsg_device *rpdev;
    struct delayed_work work;
};

static DEFINE_SPINLOCK(health_lock);
static struct amp_health_state health;
static struct amp_health_peer *owner;

static unsigned long long health_now(void)
{
    return ktime_to_ms(ktime_get_boottime());
}

static const char *phase_name(enum amp_health_phase phase)
{
    switch (phase) {
    case AMP_HEALTH_WAIT_ACK: return "WAIT_ACK";
    case AMP_HEALTH_READY: return "READY";
    case AMP_HEALTH_WAIT_PONG: return "WAIT_PONG";
    case AMP_HEALTH_FAILED: return "FAILED";
    case AMP_HEALTH_DONE: return "DONE";
    }
    return "INVALID";
}

static int get_health(char *buffer, const struct kernel_param *parameter)
{
    struct amp_health_state snapshot;
    unsigned long flags;
    bool bound;
    unsigned long long now;

    (void)parameter;
    spin_lock_irqsave(&health_lock, flags);
    snapshot = health;
    bound = owner != NULL;
    spin_unlock_irqrestore(&health_lock, flags);
    now = health_now();
    return scnprintf(buffer, PAGE_SIZE,
        "bound=%u phase=%s hello_ack=%u ping=%u pong=%u timeout=%u error=%u "
        "elapsed_ms=%llu last_pong_age_ms=%llu rtt_ms=%llu window_ms=%llu\n",
        bound, bound ? phase_name(snapshot.phase) : "UNBOUND", snapshot.hello_ack,
        snapshot.ping_count, snapshot.pong_count, snapshot.timeout_count,
        snapshot.error_count, now - snapshot.started_ms,
        snapshot.pong_count ? now - snapshot.last_pong_ms : 0,
        snapshot.rtt_ms, AMP_HEALTH_WINDOW_MS);
}

static const struct kernel_param_ops health_ops = { .get = get_health };
module_param_cb(health_status, &health_ops, NULL, 0444);
MODULE_PARM_DESC(health_status, "Read-only real test peer round-trip counters");

static void health_work(struct work_struct *work)
{
    struct amp_health_peer *peer = container_of(to_delayed_work(work),
                                               struct amp_health_peer, work);
    unsigned long flags;
    bool send, terminal;
    int rc;
    static char ping[] = "PING";

    spin_lock_irqsave(&health_lock, flags);
    if (owner != peer) {
        spin_unlock_irqrestore(&health_lock, flags);
        return;
    }
    send = amp_health_poll(&health, health_now());
    terminal = health.phase == AMP_HEALTH_FAILED || health.phase == AMP_HEALTH_DONE;
    spin_unlock_irqrestore(&health_lock, flags);
    if (send) {
        /* Never wait for a free transport buffer from a health worker. */
        rc = rpmsg_trysend(peer->rpdev->ept, ping, sizeof(ping) - 1);
        if (rc) {
            spin_lock_irqsave(&health_lock, flags);
            amp_health_fail(&health);
            spin_unlock_irqrestore(&health_lock, flags);
            dev_err(&peer->rpdev->dev, "SI health trysend error=%d; stop\n", rc);
            return;
        }
    }
    if (terminal) {
        dev_info(&peer->rpdev->dev, "SI health bounded worker stopped; read health_status\n");
        return;
    }
    schedule_delayed_work(&peer->work, msecs_to_jiffies(100));
}

static int health_probe(struct rpmsg_device *rpdev)
{
    struct amp_health_peer *peer;
    unsigned long flags;
    int rc;
    static char hello[] = "HELLO";

    peer = kzalloc(sizeof(*peer), GFP_KERNEL);
    if (!peer)
        return -ENOMEM;
    peer->rpdev = rpdev;
    INIT_DELAYED_WORK(&peer->work, health_work);
    spin_lock_irqsave(&health_lock, flags);
    if (owner) {
        spin_unlock_irqrestore(&health_lock, flags);
        kfree(peer);
        return -EBUSY;
    }
    memset(&health, 0, sizeof(health));
    health.phase = AMP_HEALTH_WAIT_ACK;
    health.started_ms = health_now();
    health.observed_ms = health.started_ms;
    health.deadline_ms = health.started_ms + AMP_HEALTH_REPLY_MS;
    owner = peer;
    spin_unlock_irqrestore(&health_lock, flags);
    dev_set_drvdata(&rpdev->dev, peer);
    dev_info(&rpdev->dev, "SI health TEST_ONLY src=%u dst=%u window_ms=%llu\n",
             rpdev->src, rpdev->dst, AMP_HEALTH_WINDOW_MS);
    rc = rpmsg_trysend(rpdev->ept, hello, sizeof(hello) - 1);
    if (rc) {
        spin_lock_irqsave(&health_lock, flags);
        amp_health_fail(&health);
        owner = NULL;
        spin_unlock_irqrestore(&health_lock, flags);
        dev_set_drvdata(&rpdev->dev, NULL);
        kfree(peer);
        return rc;
    }
    schedule_delayed_work(&peer->work, msecs_to_jiffies(100));
    return 0;
}

static int health_callback(struct rpmsg_device *rpdev, void *data, int len,
                           void *priv, u32 src)
{
    unsigned long flags;
    int kind = 0;
    (void)priv;
    if (src == rpdev->dst && len == 9 && !memcmp(data, "HELLO_ACK", 9))
        kind = 1;
    else if (src == rpdev->dst && len == 4 && !memcmp(data, "PONG", 4))
        kind = 2;
    spin_lock_irqsave(&health_lock, flags);
    if (owner && owner->rpdev == rpdev)
        amp_health_receive(&health, kind, health_now());
    spin_unlock_irqrestore(&health_lock, flags);
    return 0;
}

static void health_remove(struct rpmsg_device *rpdev)
{
    struct amp_health_peer *peer = dev_get_drvdata(&rpdev->dev);
    unsigned long flags;
    if (!peer)
        return;
    spin_lock_irqsave(&health_lock, flags);
    if (owner == peer)
        owner = NULL;
    spin_unlock_irqrestore(&health_lock, flags);
    cancel_delayed_work_sync(&peer->work);
    dev_set_drvdata(&rpdev->dev, NULL);
    kfree(peer);
}

static const struct rpmsg_device_id health_ids[] = {
    { .name = AMP_HEALTH_SERVICE }, { }
};
MODULE_DEVICE_TABLE(rpmsg, health_ids);
static struct rpmsg_driver health_driver = {
    .drv.name = "rk3576_amp_health_test",
    .id_table = health_ids,
    .probe = health_probe,
    .callback = health_callback,
    .remove = health_remove,
};
module_rpmsg_driver(health_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bounded original HELLO/PING/PONG coexistence test only");
