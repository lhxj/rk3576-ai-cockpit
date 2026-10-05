// SPDX-License-Identifier: GPL-2.0
/* Bounded coexistence health TEST. No new wire protocol or transport changes. */
#include <linux/ktime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include "amp_health_state.h"
#include "resource_probe_state.h"

#define AMP_HEALTH_SERVICE "rk3576-m0-echo"

struct amp_health_peer {
    struct rpmsg_device *rpdev;
    struct delayed_work work;
    struct clk *resource_clk[2];
    struct device *amp_owner;
    int probe_state; /* 0 unsent, 1 wait, 2 done, 3 stop */
    unsigned long long probe_deadline;
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
    int probe_state;
    unsigned long long now;

    (void)parameter;
    spin_lock_irqsave(&health_lock, flags);
    snapshot = health;
    bound = owner != NULL;
    probe_state = owner ? owner->probe_state : 0;
    spin_unlock_irqrestore(&health_lock, flags);
    now = health_now();
    return scnprintf(buffer, PAGE_SIZE,
        "resource_probe_state=%d bound=%u phase=%s hello_ack=%u ping=%u pong=%u timeout=%u error=%u "
        "elapsed_ms=%llu last_pong_age_ms=%llu rtt_ms=%llu window_ms=%llu\n",
        probe_state, bound, bound ? phase_name(snapshot.phase) : "UNBOUND", snapshot.hello_ack,
        snapshot.ping_count, snapshot.pong_count, snapshot.timeout_count,
        snapshot.error_count, now - snapshot.started_ms,
        snapshot.pong_count ? now - snapshot.last_pong_ms : 0,
        snapshot.rtt_ms, AMP_HEALTH_WINDOW_MS);
}

static const struct kernel_param_ops health_ops = { .get = get_health };
module_param_cb(health_status, &health_ops, NULL, 0444);
MODULE_PARM_DESC(health_status, "Read-only real test peer round-trip counters");

static void resource_release(struct amp_health_peer *peer)
{
    int i;
    for (i=1;i>=0;i--) {
        if (!IS_ERR_OR_NULL(peer->resource_clk[i])) {
            if (i==1) clk_rate_exclusive_put(peer->resource_clk[i]);
            clk_disable_unprepare(peer->resource_clk[i]);
            clk_put(peer->resource_clk[i]);
            peer->resource_clk[i]=NULL;
        }
    }
    if (peer->amp_owner) put_device(peer->amp_owner);
    peer->amp_owner=NULL;
}
static int resource_acquire(struct amp_health_peer *peer)
{
    struct device_node *node=of_find_node_by_path("/mcu-amp");
    struct platform_device *amp;
    struct clk *c,*parent;
    int i,rc;
    if (!node) return -ENODEV;
    amp=of_find_device_by_node(node);
    if (!amp) { of_node_put(node); return -ENODEV; }
    peer->amp_owner=&amp->dev;
    if (!amp->dev.driver || strcmp(amp->dev.driver->name,"rockchip-amp") ||
        of_clk_get_parent_count(node)!=7) { rc=-EINVAL; goto fail; }
    for(i=0;i<2;i++) {
        c=of_clk_get(node,5+i);
        if (IS_ERR(c)) { rc=PTR_ERR(c); goto fail; }
        if (strcmp(__clk_get_name(c),i ? "clk_i2c9" : "pclk_i2c9")) {
            clk_put(c); rc=-EINVAL; goto fail;
        }
        rc=clk_prepare_enable(c);
        if(rc) { clk_put(c); goto fail; }
        if(i==1) {
            rc=clk_rate_exclusive_get(c);
            if(rc) { clk_disable_unprepare(c); clk_put(c); goto fail; }
        }
        peer->resource_clk[i]=c;
    }
    parent=clk_get_parent(peer->resource_clk[1]);
    if(!parent || strcmp(__clk_get_name(parent),"xin24m") ||
       clk_get_rate(peer->resource_clk[1])!=24000000UL ||
       !__clk_is_enabled(peer->resource_clk[0]) ||
       !__clk_is_enabled(peer->resource_clk[1])) { rc=-EINVAL; goto fail; }
    dev_info(&peer->rpdev->dev,"RESOURCE_OWNER_READY I2C9 clocks held parent=xin24m rate=24000000; no I2C transactions\n");
    of_node_put(node);
    return 0;
fail:
    of_node_put(node);
    resource_release(peer);
    return rc;
}

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
    if (peer->probe_state==1) {
        probe_expire(&peer->probe_state,peer->probe_deadline,health_now());
        if (peer->probe_state==3) {
            amp_health_fail(&health);
        }
        spin_unlock_irqrestore(&health_lock,flags);
        if(READ_ONCE(peer->probe_state)==1) schedule_delayed_work(&peer->work,msecs_to_jiffies(100));
        return;
    }
    if (!peer->probe_state && health.pong_count && health.phase==AMP_HEALTH_READY) {
        static char command[]="RESOURCE_PROBE_V1";
        probe_begin(&peer->probe_state,&peer->probe_deadline,health_now());
        spin_unlock_irqrestore(&health_lock,flags);
        rc=rpmsg_trysend(peer->rpdev->ept,command,sizeof(command)-1);
        if(rc) {
            spin_lock_irqsave(&health_lock,flags);
            peer->probe_state=3; amp_health_fail(&health);
            spin_unlock_irqrestore(&health_lock,flags);
            return;
        }
        dev_info(&peer->rpdev->dev,"RESOURCE_PROBE_V1 command sent once deadline_ms=3000\n");
        schedule_delayed_work(&peer->work,msecs_to_jiffies(100));
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
            dev_err(&peer->rpdev->dev, "Resource probe health trysend error=%d; stop\n", rc);
            return;
        }
    }
    if (terminal) {
        dev_info(&peer->rpdev->dev, "Resource probe health bounded worker stopped; read health_status\n");
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
    rc=resource_acquire(peer);
    if(rc) { kfree(peer); return rc; }
    spin_lock_irqsave(&health_lock, flags);
    if (owner) {
        spin_unlock_irqrestore(&health_lock, flags);
        resource_release(peer);
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
    dev_info(&rpdev->dev, "Resource probe health TEST_ONLY src=%u dst=%u window_ms=%llu\n",
             rpdev->src, rpdev->dst, AMP_HEALTH_WINDOW_MS);
    rc = rpmsg_trysend(rpdev->ept, hello, sizeof(hello) - 1);
    if (rc) {
        spin_lock_irqsave(&health_lock, flags);
        amp_health_fail(&health);
        owner = NULL;
        spin_unlock_irqrestore(&health_lock, flags);
        dev_set_drvdata(&rpdev->dev, NULL);
        resource_release(peer);
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
    if(owner && owner->rpdev==rpdev && owner->probe_state==1 &&
       src==rpdev->dst && len==22 &&
       (!memcmp(data,"RESOURCE_PROBE_DONE_V1",22) || !memcmp(data,"RESOURCE_PROBE_STOP_V1",22))) {
        probe_receive(&owner->probe_state,owner->probe_deadline,health_now(),!memcmp(data,"RESOURCE_PROBE_DONE_V1",22));
        if(owner->probe_state==3) amp_health_fail(&health);
    } else if (owner && owner->rpdev == rpdev)
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
    resource_release(peer);
    kfree(peer);
}

static const struct rpmsg_device_id health_ids[] = {
    { .name = AMP_HEALTH_SERVICE }, { }
};
MODULE_DEVICE_TABLE(rpmsg, health_ids);
static struct rpmsg_driver health_driver = {
    .drv.name = "rk3576_i2c_resource_probe",
    .id_table = health_ids,
    .probe = health_probe,
    .callback = health_callback,
    .remove = health_remove,
};
module_rpmsg_driver(health_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bounded I2C_RESOURCE_PROBE_V1 read-only M0 permission diagnostic");
