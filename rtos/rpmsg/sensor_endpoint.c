/* SPDX-License-Identifier: Apache-2.0 */
#include "sensor_endpoint.h"
#include "sensor_service.h"
#include "sensor_task.h"
#include "rpmsg_ns.h"
#include "amp_address.h"
#include <rthw.h>
#include <string.h>
#define CONTROL_CAPACITY 8u
struct control_item { uint32_t peer,length;uint8_t wire[SV_MAX_WIRE]; };
static struct control_item queue[CONTROL_CAPACITY];
static unsigned head,tail,queued,control_drops;
static struct mpu_sample_state mailbox;
static unsigned mailbox_dirty,mailbox_overwrites;
static struct sensor_service service;
static struct rpmsg_lite_instance *instance;
static struct rpmsg_lite_endpoint *endpoint;
static struct rt_thread worker;
static uint8_t worker_stack[4096];
static struct rt_semaphore exited;
static volatile unsigned stopping;
static volatile unsigned fatal_pool;
static unsigned worker_initialized,endpoint_claimed,announced;
static uint64_t ticks;
static rt_tick_t last_tick;
static uint64_t now_ms(void) {
 rt_tick_t now=rt_tick_get();ticks+=(rt_tick_t)(now-last_tick);last_tick=now;
 return ticks*1000/RT_TICK_PER_SECOND;
}
/* Callback owns no I2C, send, allocation, blocking operation or service mutation. */
static int32_t receive(void *data,uint32_t length,uint32_t peer,void *private) {
 uint32_t pa=amp_pool_m0_to_pa((uintptr_t)data);
 rt_base_t irq=rt_hw_interrupt_disable();(void)private;
 if(!pa || pa-AMP_POOL_LINUX_PA<16u || length>RL_BUFFER_PAYLOAD_SIZE || length>AMP_POOL_SIZE-(pa-AMP_POOL_LINUX_PA)) {
  fatal_pool=1;control_drops++;rt_hw_interrupt_enable(irq);return RL_HOLD;
 }
 if(!stopping && length>=SV_HEADER_SIZE && length<=SV_MAX_WIRE && queued<CONTROL_CAPACITY) {
  queue[tail].peer=peer;queue[tail].length=length;memcpy(queue[tail].wire,data,length);
  tail=(tail+1)%CONTROL_CAPACITY;queued++;
 } else control_drops++;
 rt_hw_interrupt_enable(irq);return RL_RELEASE;
}
void mpu_sensor_report(const struct mpu_sample_state *sample) {
 rt_base_t irq=rt_hw_interrupt_disable();
 if(mailbox_dirty)mailbox_overwrites++;
 mailbox=*sample;mailbox_dirty=1;
 rt_hw_interrupt_enable(irq);
}
static int send_wire(void *unused,uint32_t peer,const uint8_t *wire,size_t length) {
 (void)unused;
 return rpmsg_lite_send(instance,endpoint,peer,(char*)wire,(uint32_t)length,RL_DONT_BLOCK)==RL_SUCCESS ? 0:-1;
}
static int initialize(void *unused,uint64_t epoch) {
 (void)unused;return mpu_sensor_resource_ready(epoch)==RT_EOK ? 0:(int)mpu_sensor_last_error();
}
static void run(void *unused) {
 struct sensor_service_io io={0,send_wire,initialize};(void)unused;
 sensor_service_init(&service,io,0);
 while(!stopping) {
  struct control_item item;struct mpu_sample_state sample;unsigned got_item=0,got_sample=0,drops;
  rt_base_t irq=rt_hw_interrupt_disable();
  if(queued){item=queue[head];head=(head+1)%CONTROL_CAPACITY;queued--;got_item=1;}
  if(mailbox_dirty){sample=mailbox;mailbox_dirty=0;got_sample=1;}
  drops=control_drops;
  service.overwrites+=mailbox_overwrites;mailbox_overwrites=0;
  rt_hw_interrupt_enable(irq);
  if(got_item)(void)sensor_service_request(&service,item.peer,item.wire,item.length,now_ms());
  if(got_sample)sensor_service_offer(&service,&sample);
  /* Queue/mailbox loss is counted separately from transport SAMPLE sequence gaps. */
  if(service.control_drops!=drops)service.status_dirty=1;
  service.control_drops=drops;
  sensor_service_poll(&service,now_ms());
  if(service.stopped || fatal_pool) {mpu_sensor_request_stop();break;}
  rt_thread_mdelay(10);
 }
 mpu_sensor_request_stop();rt_sem_release(&exited);
}
/* Standard Linux RPMsg NS uses little-endian addr/flags after name[32].
 * Encode its existing 40-byte envelope explicitly and send once without RL_BLOCK. */
static int announce(uint32_t flags) {
 uint8_t bytes[40]={0};unsigned i;uint32_t address=0x3005;
 memcpy(bytes,"rk3576-sensor-v1",16);
 for(i=0;i<4;i++){bytes[32+i]=(uint8_t)(address>>(8*i));bytes[36+i]=(uint8_t)(flags>>(8*i));}
 return rpmsg_lite_send(instance,endpoint,RL_NS_EPT_ADDR,(char*)bytes,sizeof(bytes),RL_DONT_BLOCK);
}
rt_err_t sensor_endpoint_start(struct rpmsg_lite_instance *shared_instance,rt_tick_t owner_start) {
 rt_err_t result;
 if(!shared_instance||instance||endpoint_claimed)return -RT_EINVAL;
 endpoint_claimed=1;
 instance=shared_instance;last_tick=owner_start;
 result=rt_sem_init(&exited,"sensor-exit",0,RT_IPC_FLAG_FIFO);
 if(result!=RT_EOK){instance=0;return result;}
 endpoint=rpmsg_lite_create_ept(instance,0x3005,receive,0);
 if(!endpoint){rt_sem_detach(&exited);instance=0;return -RT_ENOMEM;}
 result=rt_thread_init(&worker,"sensor-control",run,0,worker_stack,sizeof(worker_stack),13,10);
 if(result!=RT_EOK)goto fail;
 worker_initialized=1;
 if(announce(RL_NS_CREATE)!=RL_SUCCESS){result=-RT_ERROR;goto fail;}
 announced=1;
 result=rt_thread_startup(&worker);
 if(result==RT_EOK)return result;
fail:
 if(announced)(void)announce(RL_NS_DESTROY);
 if(worker_initialized)rt_thread_detach(&worker);
 rpmsg_lite_destroy_ept(instance,endpoint);endpoint=0;rt_sem_detach(&exited);instance=0;return result;
}
rt_bool_t sensor_endpoint_stop(void) {
 rt_tick_t start;
 if(!instance)return RT_TRUE;
 if(fatal_pool)return RT_FALSE;
 stopping=1;
 if(rt_sem_take(&exited,rt_tick_from_millisecond(1000))!=RT_EOK)return RT_FALSE;
 start=rt_tick_get();
 while(!mpu_sensor_has_stopped()) {
  if((rt_tick_t)(rt_tick_get()-start)>=rt_tick_from_millisecond(1000))return RT_FALSE;
  rt_thread_mdelay(10);
 }
 (void)announce(RL_NS_DESTROY);
 rpmsg_lite_destroy_ept(instance,endpoint);endpoint=0;rt_sem_detach(&exited);instance=0;
 return RT_TRUE;
}
