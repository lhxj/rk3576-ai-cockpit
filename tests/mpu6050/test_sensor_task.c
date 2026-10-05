/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../../rtos/sensor/sensor_task.c"
static rt_tick_t tick;
static unsigned transfers,ready_calls,delays,config_lines,raw_lines;
static int fail_samples,fail_setup,bad_config;
static uint8_t registers[128];
static struct rt_i2c_bus_device fake_bus;
static void (*entry)(void*);
rt_tick_t rt_tick_get(void){return tick;}
rt_tick_t rt_tick_from_millisecond(int ms){return (ms*RT_TICK_PER_SECOND+999)/1000;}
int rt_interrupt_get_nest(void){return 0;}
rt_base_t rt_hw_interrupt_disable(void){return 0;}
void rt_hw_interrupt_enable(rt_base_t v){(void)v;}
void rt_thread_mdelay(int ms){assert(ms>0);tick+=rt_tick_from_millisecond(ms);delays++;}
int rt_kprintf(const char*fmt,...){
 char buffer[128];va_list args;va_start(args,fmt);int length=vsnprintf(buffer,sizeof(buffer)-1,fmt,args);va_end(args);
 assert(length>=0&&length<=126&&buffer[length-1]=='\n');
 if(!strncmp(fmt,"MPU_CONFIG ",11))config_lines++;
 if(!strncmp(fmt,"MPU_RAW ",8))raw_lines++;
 return length;
}
rt_err_t rockchip_i2c9_resource_ready(rt_tick_t t){assert(t==rt_tick_from_millisecond(20));ready_calls++;return fail_setup?-RT_ERROR:0;}
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char*n){assert(!strcmp(n,"i2c9"));return &fake_bus;}
rt_size_t rt_i2c_transfer(struct rt_i2c_bus_device*b,struct rt_i2c_msg*m,rt_uint32_t n){
 assert(b==&fake_bus&&m[0].addr==0x68);transfers++;
 if(n==2 && m[1].len==14 && fail_samples)return (rt_size_t)HAL_ERROR;
 if(n==2){assert(m[0].len==1&&m[1].flags==RT_I2C_RD);memcpy(m[1].buf,registers+*m[0].buf,m[1].len);if(bad_config && *m[0].buf==0x1a)m[1].buf[0]^=1;}
 else {assert(n==1&&m[0].len==2);registers[m[0].buf[0]]=m[0].buf[1];if(m[0].buf[0]==0x6b&&m[0].buf[1]==0x80)registers[0x6b]=0x40;}
 tick+=rt_tick_from_millisecond(2);return n;
}
rt_err_t rt_thread_init(struct rt_thread*t,const char*n,void(*fn)(void*),void*p,void*stack,rt_uint32_t size,rt_uint8_t priority,rt_uint32_t slice){
 (void)t;(void)p;(void)priority;(void)slice;assert(!strcmp(n,"sensor")&&stack==task_stack&&size==2048);entry=fn;return 0;
}
rt_err_t rt_thread_startup(struct rt_thread*t){(void)t;return 0;}
int main(int argc,char**argv){
 assert(!transfers&&!ready_calls&&!entry);
 assert(mpu_sensor_resource_ready(0)==-RT_EINVAL&&!transfers);
 registers[0x75]=0x68;
 if(argc>1){
  if(!strcmp(argv[1],"who"))registers[0x75]=0x69;
  if(!strcmp(argv[1],"config"))bad_config=1;
  if(!strcmp(argv[1],"setup"))fail_setup=1;
  assert(mpu_sensor_resource_ready(123)==-RT_ERROR&&!entry&&!sampler_running&&state.errors==1&&!state.valid);
  assert(state.error==(fail_setup?MPU_UNAVAILABLE:(bad_config?MPU_BAD_CONFIG:MPU_WRONG_ID)));
  return 0;
 }
 assert(mpu_sensor_resource_ready(123)==0&&ready_calls==1&&entry);
 for(unsigned i=0;i<14;i+=2){registers[0x3b+i]=0x80;registers[0x3b+i+1]=0;}
 unsigned before=transfers;
 assert(mpu_sensor_resource_ready(123)==-RT_EBUSY&&mpu_sensor_resource_ready(124)==-RT_EINVAL&&transfers==before&&ready_calls==1);
 rt_tick_t begin=tick;entry(0);
 assert(state.attempts==100&&state.sample_seq==100&&!state.errors&&state.valid);
 assert(transfers-before==100&&delays>=102&&tick-begin==rt_tick_from_millisecond(5000));
 assert(max_interval_ms==50&&missed_periods==0);
#ifdef MPU_SENSOR_TRACE_V1
 assert(config_lines==1&&raw_lines==100&&raw_trace_count==100);
#else
 assert(!config_lines&&!raw_lines);
#endif
 assert(transfer_result((rt_size_t)-RT_ETIMEOUT,2,14)==-MPU_TIMEOUT);
 assert(transfer_result((rt_size_t)HAL_ERROR,2,14)==-MPU_UNAVAILABLE);
 uint64_t last_time=state.last_valid_ms,last_seq=state.sample_seq;
 before=transfers;fail_samples=1;entry(0);
 assert(transfers-before==3 && state.errors==3 && !state.valid && state.last_valid_ms==last_time && state.sample_seq==last_seq);
#ifdef MPU_SENSOR_TRACE_V1
 assert(config_lines==1&&raw_lines==100);
#endif
 puts("MPU_SENSOR_TASK_FAKE_PASS");return 0;
}
