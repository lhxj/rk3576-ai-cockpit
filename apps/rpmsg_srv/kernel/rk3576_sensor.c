// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/kref.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_platform.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "sensor_uapi.h"
#define WIRE_MAX 256
#define CONTROL_CAP 8
struct sensor_packet { u16 len; u8 data[WIRE_MAX]; };
struct sensor_peer {
 struct rpmsg_device *rpdev;
 struct miscdevice misc;
 struct kref ref;
 struct work_struct free_work;
 struct mutex io;
 spinlock_t lock;
 wait_queue_head_t wait;
 bool opened, removed, sample_pending;
 unsigned head, count;
 struct sensor_packet control[CONTROL_CAP], sample;
 struct sensor_link_state state;
 struct clk *resource_clk[2];
 struct device *amp_owner;
};
static struct workqueue_struct *release_queue;
static DEFINE_SPINLOCK(callback_lock);
static struct sensor_peer *callback_peer;
static atomic64_t generations = ATOMIC64_INIT(0);
static void resource_release(struct sensor_peer *peer)
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
static int resource_acquire(struct sensor_peer *peer)
{
    struct device_node *node=of_find_node_by_path("/mcu-amp");
    struct platform_device *amp;
    struct clk *c,*parent;
    int i,rc;
    struct device_node *i2c, *pins;
    u32 cells[8];
    if (!node) return -ENODEV;
    i2c=of_find_node_by_path("/i2c@2ae80000");
    if (!i2c || of_device_is_available(i2c) || of_get_child_count(i2c)) {
        of_node_put(i2c); of_node_put(node); return -EINVAL;
    }
    amp=of_find_device_by_node(i2c);
    if (amp) { put_device(&amp->dev); of_node_put(i2c); of_node_put(node); return -EBUSY; }
    of_node_put(i2c);
    pins=of_parse_phandle(node,"pinctrl-0",1);
    if (!pins || of_property_count_u32_elems(pins,"rockchip,pins")!=8 || of_property_read_u32_array(pins,"rockchip,pins",cells,8) ||
        cells[0]!=1 || cells[1]!=13 || cells[2]!=10 ||
        cells[4]!=1 || cells[5]!=12 || cells[6]!=10) {
        of_node_put(pins); of_node_put(node); return -EINVAL;
    }
    of_node_put(pins);
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


static void peer_release_work(struct work_struct *work)
{
 struct sensor_peer *p=container_of(work,struct sensor_peer,free_work);
 resource_release(p);kfree(p);
}
static void peer_free(struct kref *ref)
{
 struct sensor_peer *p=container_of(ref,struct sensor_peer,ref);
 /* Last reference can be a callback in IRQ; sleeping CCF release is deferred. */
 queue_work(release_queue,&p->free_work);
}
static int sensor_open(struct inode *inode,struct file *file)
{
 struct sensor_peer *p=container_of(file->private_data,struct sensor_peer,misc);
 int rc=0;unsigned long flags;
 mutex_lock(&p->io);
 if(p->removed) rc=-ENODEV;
 else if(p->opened) rc=-EBUSY;
 else { spin_lock_irqsave(&p->lock,flags);p->opened=true;spin_unlock_irqrestore(&p->lock,flags); kref_get(&p->ref); file->private_data=p; }
 mutex_unlock(&p->io);
 return rc;
}
static int sensor_close(struct inode *inode,struct file *file)
{
 struct sensor_peer *p=file->private_data;
 unsigned long flags;
 mutex_lock(&p->io);
 spin_lock_irqsave(&p->lock,flags);
 p->opened=false; p->head=p->count=0; p->sample_pending=false;
 spin_unlock_irqrestore(&p->lock,flags);
 mutex_unlock(&p->io);
 kref_put(&p->ref,peer_free);
 return 0;
}
static bool readable(struct sensor_peer *p)
{
 return READ_ONCE(p->removed)||READ_ONCE(p->count)||READ_ONCE(p->sample_pending);
}
static ssize_t sensor_read(struct file *file,char __user *buf,size_t size,loff_t *offset)
{
 struct sensor_peer *p=file->private_data;
 struct sensor_packet packet;
 bool control=false;
 unsigned long flags;
 int rc;
 if(!(file->f_flags&O_NONBLOCK)) {
  rc=wait_event_interruptible(p->wait,readable(p)); if(rc)return rc;
 }
 mutex_lock(&p->io);
 spin_lock_irqsave(&p->lock,flags);
 if(p->removed) rc=-ENODEV;
 else if(!p->count&&!p->sample_pending) rc=-EAGAIN;
 else {
  control=p->count!=0;
  packet=control?p->control[p->head]:p->sample;
  if(size<packet.len) rc=-EMSGSIZE;
  else rc=0;
 }
 spin_unlock_irqrestore(&p->lock,flags);
 if(!rc) {
  if(copy_to_user(buf,packet.data,packet.len)) rc=-EFAULT;
  else {
   spin_lock_irqsave(&p->lock,flags);
   /* callback may replace sample during copy: preserve the new latest sample. */
   if(control) { p->head=(p->head+1)%CONTROL_CAP; p->count--; }
   else if(p->sample.len==packet.len&&!memcmp(p->sample.data,packet.data,packet.len)) p->sample_pending=false;
   spin_unlock_irqrestore(&p->lock,flags);
   rc=packet.len;
  }
 }
 mutex_unlock(&p->io);
 return rc;
}
static ssize_t sensor_write(struct file *file,const char __user *buf,size_t size,loff_t *offset)
{
 struct sensor_peer *p=file->private_data;
 u8 wire[WIRE_MAX]; int rc; unsigned long flags;
 if(size<44||size>WIRE_MAX)return -EMSGSIZE;
 if(copy_from_user(wire,buf,size))return -EFAULT;
 mutex_lock(&p->io);
 if(p->removed||!p->state.owner_ready)rc=-ENODEV;
 else rc=rpmsg_trysend(p->rpdev->ept,wire,size);
 if(rc) {spin_lock_irqsave(&p->lock,flags);p->state.send_failures++;spin_unlock_irqrestore(&p->lock,flags);}
 mutex_unlock(&p->io);
 return rc?rc:size;
}
static __poll_t sensor_poll(struct file *file,poll_table *table)
{
 struct sensor_peer *p=file->private_data;
 __poll_t mask=0; unsigned long flags;
 poll_wait(file,&p->wait,table);
 spin_lock_irqsave(&p->lock,flags);
 if(p->removed)mask=EPOLLHUP|EPOLLERR;
 else {if(p->count||p->sample_pending)mask|=EPOLLIN|EPOLLRDNORM;mask|=EPOLLOUT|EPOLLWRNORM;}
 spin_unlock_irqrestore(&p->lock,flags);
 return mask;
}
static long sensor_ioctl(struct file *file,unsigned int command,unsigned long arg)
{
 struct sensor_peer *p=file->private_data;
 struct sensor_link_state state; unsigned long flags;
 if(command!=SENSOR_IOC_STATE)return -ENOTTY;
 spin_lock_irqsave(&p->lock,flags);state=p->state;spin_unlock_irqrestore(&p->lock,flags);
 return copy_to_user((void __user *)arg,&state,sizeof(state))?-EFAULT:0;
}
static const struct file_operations sensor_fops={.owner=THIS_MODULE,.open=sensor_open,.release=sensor_close,
 .read=sensor_read,.write=sensor_write,.poll=sensor_poll,.unlocked_ioctl=sensor_ioctl,.llseek=no_llseek};
static int sensor_callback(struct rpmsg_device *rpdev,void *data,int len,void *priv,u32 src)
{
 struct sensor_peer *p;
 struct sensor_packet *packet; unsigned long flags;
 bool sample;
 spin_lock_irqsave(&callback_lock,flags);
 p=callback_peer;
 if(p&&p->rpdev==rpdev) kref_get(&p->ref); else p=NULL;
 spin_unlock_irqrestore(&callback_lock,flags);
 if(!p)return 0;
 spin_lock_irqsave(&p->lock,flags);
 if(p->removed)goto out;
 if(src!=rpdev->dst||len<44||len>WIRE_MAX) {p->state.malformed++;goto out;}
 if(!p->opened)goto out;
 sample=((u8 *)data)[6]==1&&((u8 *)data)[7]==4;
 if(sample) {if(p->sample_pending)p->state.sample_overwrites++;packet=&p->sample;p->sample_pending=true;}
 else {if(p->count==CONTROL_CAP){p->state.control_drops++;goto out;}packet=&p->control[(p->head+p->count++)%CONTROL_CAP];}
 packet->len=len;memcpy(packet->data,data,len);
out:
 spin_unlock_irqrestore(&p->lock,flags);wake_up_interruptible(&p->wait);kref_put(&p->ref,peer_free);return 0;
}
static int sensor_probe(struct rpmsg_device *rpdev)
{
 struct sensor_peer *p;int rc;unsigned long flags;
 p=kzalloc(sizeof(*p),GFP_KERNEL);if(!p)return -ENOMEM;
 p->rpdev=rpdev;INIT_WORK(&p->free_work,peer_release_work);mutex_init(&p->io);spin_lock_init(&p->lock);init_waitqueue_head(&p->wait);kref_init(&p->ref);
 rc=resource_acquire(p);if(rc){kfree(p);return rc;}
 p->state.abi=SENSOR_ABI;p->state.owner_ready=1;p->state.generation=atomic64_inc_return(&generations);
 p->misc.minor=MISC_DYNAMIC_MINOR;p->misc.name="rk3576-sensor-v1";p->misc.fops=&sensor_fops;p->misc.mode=0600;
 dev_set_drvdata(&rpdev->dev,p);
 rc=misc_register(&p->misc);if(rc){dev_set_drvdata(&rpdev->dev,NULL);kref_put(&p->ref,peer_free);}
 if(!rc) {
  spin_lock_irqsave(&callback_lock,flags);
  if(callback_peer)rc=-EBUSY;else callback_peer=p;
  spin_unlock_irqrestore(&callback_lock,flags);
  if(rc){misc_deregister(&p->misc);dev_set_drvdata(&rpdev->dev,NULL);kref_put(&p->ref,peer_free);}
 }
 return rc;
}
static void sensor_remove(struct rpmsg_device *rpdev)
{
 struct sensor_peer *p=dev_get_drvdata(&rpdev->dev);unsigned long flags;
 if(!p)return;
 spin_lock_irqsave(&callback_lock,flags);if(callback_peer==p)callback_peer=NULL;spin_unlock_irqrestore(&callback_lock,flags);
 misc_deregister(&p->misc);
 mutex_lock(&p->io);spin_lock_irqsave(&p->lock,flags);
 p->removed=true;p->state.removed=1;p->state.owner_ready=0;p->count=0;p->sample_pending=false;
 spin_unlock_irqrestore(&p->lock,flags);mutex_unlock(&p->io);
 wake_up_interruptible(&p->wait);
 /* Global pointer is fenced; existing callbacks/open fd retain kref and CCF holds. */
 dev_set_drvdata(&rpdev->dev,NULL);kref_put(&p->ref,peer_free);
}
static const struct rpmsg_device_id sensor_ids[]={{.name="rk3576-sensor-v1"},{}};
MODULE_DEVICE_TABLE(rpmsg,sensor_ids);
static struct rpmsg_driver sensor_driver={.drv.name="rk3576_sensor",.id_table=sensor_ids,
 .probe=sensor_probe,.callback=sensor_callback,.remove=sensor_remove};
static int __init sensor_module_init(void)
{
 int rc;release_queue=alloc_ordered_workqueue("rk3576_sensor_release",WQ_MEM_RECLAIM);
 if(!release_queue)return -ENOMEM;
 rc=register_rpmsg_driver(&sensor_driver);
 if(rc){destroy_workqueue(release_queue);release_queue=NULL;}
 return rc;
}
static void __exit sensor_module_exit(void)
{
 unregister_rpmsg_driver(&sensor_driver);
 /* Bus destroys endpoint and synchronizes callbacks before unregister returns. */
 flush_workqueue(release_queue);destroy_workqueue(release_queue);
}
module_init(sensor_module_init);module_exit(sensor_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RK3576 bounded MPU sensor RPMsg bridge; no Linux I2C transactions");
