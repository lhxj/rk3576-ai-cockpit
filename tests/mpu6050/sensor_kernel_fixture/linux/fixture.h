#ifndef SENSOR_KERNEL_FIXTURE_H
#define SENSOR_KERNEL_FIXTURE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <asm-generic/ioctl.h>
#include <stdatomic.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint32_t __u32;typedef uint64_t __u64;

#define __user
#define __init
#define __exit
#define GFP_KERNEL 0
#define WQ_MEM_RECLAIM 0
#define PAGE_SIZE 4096
#define THIS_MODULE NULL
#define O_NONBLOCK 04000
#define MISC_DYNAMIC_MINOR 255
#define EPOLLIN 1
#define EPOLLOUT 4
#define EPOLLERR 8
#define EPOLLHUP 16
#define EPOLLRDNORM 64
#define EPOLLWRNORM 256
typedef unsigned __poll_t;typedef int poll_table;
#define READ_ONCE(x) (x)
#define container_of(ptr,type,member) ((type*)((char*)(ptr)-offsetof(type,member)))
#define DEFINE_SPINLOCK(x) int x
#define spin_lock_init(x) ((void)(x))
#define spin_lock_irqsave(lock,flags) do{(void)(lock);(flags)=0;}while(0)
#define spin_unlock_irqrestore(lock,flags) do{(void)(lock);(void)(flags);}while(0)
struct mutex{int dummy;};
#define mutex_init(x) ((void)(x))
#define mutex_lock(x) ((void)(x))
#define mutex_unlock(x) ((void)(x))
typedef int spinlock_t;
typedef int wait_queue_head_t;
#define init_waitqueue_head(x) ((void)(x))
#define wake_up_interruptible(x) ((void)(x))
#define wait_event_interruptible(q,expr) ((expr)?0:-EAGAIN)
#define poll_wait(f,q,t) do{(void)(f);(void)(q);(void)(t);}while(0)
struct kref{atomic_int count;};
static void kref_init(struct kref*r){atomic_init(&r->count,1);}
static void kref_get(struct kref*r){atomic_fetch_add(&r->count,1);}
static void kref_put(struct kref*r,void(*fn)(struct kref*)){if(atomic_fetch_sub(&r->count,1)==1)fn(r);}
typedef struct{atomic_long value;}atomic64_t;
#define ATOMIC64_INIT(v) {ATOMIC_VAR_INIT(v)}
static long atomic64_inc_return(atomic64_t*a){return atomic_fetch_add(&a->value,1)+1;}
struct work_struct{void(*fn)(struct work_struct*);};struct workqueue_struct{struct work_struct*items[16];unsigned count;};
static struct workqueue_struct*alloc_ordered_workqueue(const char*n,unsigned flags){(void)n;(void)flags;return calloc(1,sizeof(struct workqueue_struct));}
#define INIT_WORK(w,f) ((w)->fn=(f))
static bool queue_work(struct workqueue_struct*q,struct work_struct*w){assert(q->count<16);q->items[q->count++]=w;return true;}
static void flush_workqueue(struct workqueue_struct*q){while(q->count){struct work_struct*w=q->items[--q->count];w->fn(w);}}
static void destroy_workqueue(struct workqueue_struct*q){flush_workqueue(q);free(q);}
static unsigned allocated,freed,clock_releases;static bool in_callback,copy_fault;
static void*kzalloc(size_t n,int flags){(void)flags;allocated++;return calloc(1,n);}
static void kfree(void*p){freed++;free(p);}
static int copy_to_user(void*dst,const void*src,size_t len){if(copy_fault)return 1;memcpy(dst,src,len);return 0;}
#define copy_from_user copy_to_user
struct device_driver{const char*name;};struct device{struct device_driver*driver;void*data;};
struct rpmsg_device{struct device dev;unsigned src,dst;void*ept;};
static void*dev_get_drvdata(struct device*d){return d->data;}
static void dev_set_drvdata(struct device*d,void*p){d->data=p;}
#define dev_info(...) ((void)0)
static void put_device(struct device*d){(void)d;assert(!in_callback);}
struct device_node{int kind;};struct platform_device{struct device dev;};
static struct device_driver amp_driver={"rockchip-amp"};static struct platform_device amp={{&amp_driver,NULL}};
static struct device_node amp_node={0},i2c_node={1},pin_node={2};
static bool wrong_dt,wrong_pins,i2c_bound;static unsigned fake_mux=10;
static struct device_node*of_find_node_by_path(const char*path){if(!strcmp(path,"/mcu-amp"))return &amp_node;if(!strcmp(path,"/i2c@2ae80000")&&!wrong_dt)return &i2c_node;return NULL;}
#define of_node_put(x) ((void)(x))
static bool of_device_is_available(struct device_node*n){(void)n;return false;}
static int of_get_child_count(struct device_node*n){(void)n;return 0;}
static struct device_node*of_parse_phandle(struct device_node*n,const char*p,int index){(void)n;(void)p;return index==1?&pin_node:NULL;}
static int of_property_count_u32_elems(struct device_node*n,const char*p){(void)n;(void)p;return 8;}
static int of_property_read_u32_array(struct device_node*n,const char*p,u32*a,int count){(void)n;(void)p;assert(count==8);u32 cells[8]={1,13,10,0x201,1,12,10,0x201};cells[2]=fake_mux;memcpy(a,cells,sizeof(cells));return wrong_pins?-EINVAL:0;}
static struct platform_device*of_find_device_by_node(struct device_node*n){return n->kind==1?(i2c_bound?&amp:NULL):&amp;}
static int of_clk_get_parent_count(struct device_node*n){(void)n;return 7;}
struct clk{const char*name;};static struct clk clocks[2]={{"pclk_i2c9"},{"clk_i2c9"}},parent={"xin24m"};
static struct clk*of_clk_get(struct device_node*n,int index){(void)n;return &clocks[index-5];}
#define IS_ERR(x) false
#define IS_ERR_OR_NULL(x) (!(x))
#define PTR_ERR(x) (-EINVAL)
static const char*__clk_get_name(struct clk*c){return c->name;}
static int clk_prepare_enable(struct clk*c){(void)c;return 0;}
static int clk_rate_exclusive_get(struct clk*c){(void)c;return 0;}
static void clk_rate_exclusive_put(struct clk*c){(void)c;assert(!in_callback);}
static void clk_disable_unprepare(struct clk*c){(void)c;assert(!in_callback);clock_releases++;}
static void clk_put(struct clk*c){(void)c;}
static struct clk*clk_get_parent(struct clk*c){(void)c;return &parent;}
static unsigned long clk_get_rate(struct clk*c){(void)c;return 24000000;}
static bool __clk_is_enabled(struct clk*c){(void)c;return true;}
static int tx_result;
static int rpmsg_trysend(void*e,const void*data,int len){(void)e;(void)data;(void)len;return tx_result;}
struct inode{int dummy;};struct file{void*private_data;unsigned f_flags;};
struct file_operations{void*owner;int(*open)(struct inode*,struct file*);int(*release)(struct inode*,struct file*);ssize_t(*read)(struct file*,char*,size_t,loff_t*);ssize_t(*write)(struct file*,const char*,size_t,loff_t*);__poll_t(*poll)(struct file*,poll_table*);long(*unlocked_ioctl)(struct file*,unsigned,unsigned long);void*llseek;};
#define no_llseek NULL
struct miscdevice{int minor;const char*name;const struct file_operations*fops;unsigned mode;};
static int misc_register(struct miscdevice*m){(void)m;return 0;}
static void misc_deregister(struct miscdevice*m){(void)m;}
struct rpmsg_device_id{char name[32];};struct rpmsg_driver{struct device_driver drv;const struct rpmsg_device_id*id_table;int(*probe)(struct rpmsg_device*);int(*callback)(struct rpmsg_device*,void*,int,void*,u32);void(*remove)(struct rpmsg_device*);};
static int register_rpmsg_driver(struct rpmsg_driver*d){(void)d;return 0;}
static void unregister_rpmsg_driver(struct rpmsg_driver*d){(void)d;}
#define MODULE_DEVICE_TABLE(a,b)
#define module_init(x)
#define module_exit(x)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)
#endif
