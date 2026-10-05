/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../rtos/rpmsg/sensor_endpoint.c"
static unsigned created,destroyed,detached,ns_create,ns_destroy,sem_detached;
static int mode;
static uint8_t payload[256];
static struct rpmsg_lite_instance fake_instance;
static struct rpmsg_lite_endpoint fake_endpoint;
rt_tick_t rt_tick_get(void){return 0;}
rt_tick_t rt_tick_from_millisecond(int n){return (rt_tick_t)n;}
void rt_thread_mdelay(int n){(void)n;}
rt_base_t rt_hw_interrupt_disable(void){return 0;}
void rt_hw_interrupt_enable(rt_base_t n){(void)n;}
rt_err_t rt_sem_init(struct rt_semaphore*s,const char*n,unsigned count,unsigned flags){(void)s;(void)n;(void)count;(void)flags;return 0;}
rt_err_t rt_sem_take(struct rt_semaphore*s,rt_tick_t t){(void)s;assert(t==1000);return mode==5?-RT_ETIMEOUT:0;}
rt_err_t rt_sem_release(struct rt_semaphore*s){(void)s;return 0;}
rt_err_t rt_sem_detach(struct rt_semaphore*s){(void)s;sem_detached++;return 0;}
rt_err_t rt_thread_detach(struct rt_thread*t){(void)t;detached++;return 0;}
rt_err_t rt_thread_init(struct rt_thread*t,const char*n,void(*fn)(void*),void*p,void*stack,rt_uint32_t size,rt_uint8_t priority,rt_uint32_t slice){(void)t;(void)n;(void)fn;(void)p;(void)priority;(void)slice;assert(stack==worker_stack&&size==4096);return mode==2?-1:0;}
rt_err_t rt_thread_startup(struct rt_thread*t){(void)t;return mode==4?-1:0;}
rt_err_t mpu_sensor_resource_ready(uint64_t e){(void)e;return 0;}
enum mpu_error mpu_sensor_last_error(void){return MPU_UNAVAILABLE;}
void mpu_sensor_request_stop(void){}
int mpu_sensor_has_stopped(void){return 1;}
uint32_t amp_pool_m0_to_pa(uintptr_t p){return p==(uintptr_t)payload?AMP_POOL_LINUX_PA+16:0;}
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(struct rpmsg_lite_instance*i,uint32_t address,int32_t(*cb)(void*,uint32_t,uint32_t,void*),void*p){(void)p;assert(i==&fake_instance&&address==0x3005&&cb==receive);created++;return mode==1?0:&fake_endpoint;}
int rpmsg_lite_destroy_ept(struct rpmsg_lite_instance*i,struct rpmsg_lite_endpoint*e){assert(i==&fake_instance&&e==&fake_endpoint);destroyed++;return 0;}
int rpmsg_lite_send(struct rpmsg_lite_instance*i,struct rpmsg_lite_endpoint*e,uint32_t peer,char*w,uint32_t n,uint32_t wait){assert(i==&fake_instance&&e==&fake_endpoint&&wait==0);if(peer==53){assert(n==40&&!memcmp(w,"rk3576-sensor-v1",16));assert((uint8_t)w[32]==5&&(uint8_t)w[33]==0x30&&!w[34]&&!w[35]);assert(!w[37]&&!w[38]&&!w[39]);if(w[36])ns_destroy++;else ns_create++;}return mode==3?-1:0;}
int main(int argc,char**argv){assert(argc==2);mode=argv[1][0]-'0';
 int result=sensor_endpoint_start(&fake_instance,0);
 assert(sensor_endpoint_start(&fake_instance,0)==-RT_EINVAL&&created==1);
 if(mode>=1&&mode<=4){assert(result!=0&&sem_detached==1);assert(destroyed==(mode!=1));if(mode==4)assert(ns_create==1&&ns_destroy==1);return 0;}
 assert(result==0&&ns_create==1);
 if(mode==6){assert(receive((void*)1,44,7,0)==RL_HOLD&&fatal_pool);assert(!sensor_endpoint_stop()&&!destroyed);return 0;}
 for(unsigned i=0;i<8;i++)assert(receive(payload,44,7,0)==RL_RELEASE);
 assert(queued==8&&control_drops==0);assert(receive(payload,44,7,0)==RL_RELEASE&&control_drops==1);
 assert(receive(payload,257,7,0)==RL_RELEASE&&control_drops==2);
 assert(receive(payload,10,7,0)==RL_RELEASE&&control_drops==3);
 if(mode==5){assert(!sensor_endpoint_stop()&&!destroyed);}else {assert(sensor_endpoint_stop()&&destroyed==1&&ns_destroy==1);}
 puts("SENSOR_ENDPOINT_FACTORY_PASS");return 0;
}
