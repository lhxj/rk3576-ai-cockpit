/* SPDX-License-Identifier: Apache-2.0 */
/* No INIT export: only the verified control worker may invoke resource_ready. */
#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>
#include "drv_i2c.h"
#include "hal_base.h"
#include "sensor_task.h"
static volatile unsigned stop_requested, sampler_running;
RT_WEAK void mpu_sensor_report(const struct mpu_sample_state *sample) {(void)sample;}
void mpu_sensor_request_stop(void) {stop_requested=1;}
int mpu_sensor_has_stopped(void) {return !sampler_running;}
static struct mpu_device sensor;
static struct mpu_sample_state state;
enum mpu_error mpu_sensor_last_error(void) {
 rt_base_t irq=rt_hw_interrupt_disable();enum mpu_error error=state.error;
 rt_hw_interrupt_enable(irq);return error;
}
static rt_err_t initialization_failure(enum mpu_error error) {
 state.error=error;state.valid=0;state.errors++;mpu_sensor_report(&state);
 return -RT_ERROR;
}
static struct rt_thread task;
static rt_uint8_t task_stack[2048];
static unsigned ready_claimed;
static uint64_t remote_epoch, mono_ticks;
static rt_tick_t previous_tick;
static uint32_t max_interval_ms, missed_periods;
#ifdef MPU_SENSOR_TRACE_V1
static unsigned raw_trace_count;
#endif
static struct rt_i2c_bus_device *bus;
static uint64_t monotonic_ms(void *unused)
{
 rt_tick_t now=rt_tick_get();
 (void)unused;
 mono_ticks+=(rt_tick_t)(now-previous_tick);
 previous_tick=now;
 return mono_ticks*1000/RT_TICK_PER_SECOND;
}
static int transfer_result(rt_size_t n,rt_uint32_t count,size_t length)
{
 if(n==count)return (int)length;
 if(n==(rt_size_t)-RT_ETIMEOUT || n==(rt_size_t)HAL_TIMEOUT)return -MPU_TIMEOUT;
 /* Fixed HAL returns HAL_ERROR for NAK and also other controller errors:
  * preserve UNAVAILABLE instead of claiming an observed NACK. */
 if(n==(rt_size_t)HAL_ERROR)return -MPU_UNAVAILABLE;
 return n<count ? -MPU_SHORT : -MPU_UNAVAILABLE;
}
static int read_register(void *unused,uint8_t address,uint8_t reg,uint8_t*out,size_t length)
{
 struct rt_i2c_msg messages[2]={{address,RT_I2C_WR,1,&reg},{address,RT_I2C_RD,(rt_uint16_t)length,out}};
 (void)unused;
 return transfer_result(rt_i2c_transfer(bus,messages,2),2,length);
}
static int write_register(void *unused,uint8_t address,uint8_t reg,const uint8_t*value,size_t length)
{
 uint8_t bytes[2]={reg,*value};
 struct rt_i2c_msg message={address,RT_I2C_WR,2,bytes};
 (void)unused;
 if(length!=1)return -MPU_UNAVAILABLE;
 return transfer_result(rt_i2c_transfer(bus,&message,1),1,length);
}
static void wait_ms(void *unused,uint32_t duration)
{
 (void)unused;rt_thread_mdelay(duration);
}
static void sample_loop(void *unused)
{
 uint64_t prior=0;
 unsigned count, consecutive_errors=0;
 (void)unused;
 /* Sampling and publication have independent bounds; UART is diagnostic only. */
 #ifdef MPU_SENSOR_SERVICE_V1
 for(count=0;count<18000 && !stop_requested;count++)
#else
 for(count=0;count<100;count++)
#endif
 {
  uint64_t start=monotonic_ms(0),done;
  enum mpu_error error=mpu_sample(&sensor,&state);
  done=monotonic_ms(0);
  consecutive_errors=error==MPU_OK ? 0 : consecutive_errors+1;
  if(prior){uint32_t interval=(uint32_t)(start-prior);if(interval>max_interval_ms)max_interval_ms=interval;}
  prior=start;
  mpu_sensor_report(&state);
#ifdef MPU_SENSOR_TRACE_V1
  if(error==MPU_OK && raw_trace_count<100)
  {
   raw_trace_count++;
   rt_kprintf("MPU_RAW n=%u seq=%u m0_ms=%u ax=%d ay=%d az=%d temp=%d gx=%d gy=%d gz=%d config=%08x\n",
       raw_trace_count,(unsigned)state.sample_seq,(unsigned)state.last_valid_ms,
       state.last.accel[0],state.last.accel[1],state.last.accel[2],state.last.temperature,
       state.last.gyro[0],state.last.gyro[1],state.last.gyro[2],(unsigned)sensor.config.config_id);
  }
#endif
  if(count<3 || count%200==0 || error!=MPU_OK)rt_kprintf("MPU_SENSOR_V1 attempt=%u seq=%u valid=%u error=%u age_ms=%u interval_max_ms=%u\n",count+1,(unsigned)state.sample_seq,state.valid,error,(unsigned)(done-state.last_valid_ms),max_interval_ms);
  if(consecutive_errors>=3)break;
  if(done-start<50)rt_thread_mdelay((rt_int32_t)(50-(done-start)));
  else {missed_periods++;rt_thread_mdelay(1);}
 }
 sampler_running=0;
 rt_kprintf("MPU_SENSOR_V1 STOP attempts=%u errors=%u missed_periods=%u publish_via_service=%u\n",state.attempts,state.errors,missed_periods,
#ifdef MPU_SENSOR_SERVICE_V1
 1u
#else
 0u
#endif
 );
}
/* epoch is a dedicated Linux getrandom nonce bound once for this M0 boot.
 * Caller is the independent sensor service worker, after Linux resource ownership
 * preflight; never invoke hardware setup from the RPMsg callback. */
rt_err_t mpu_sensor_resource_ready(uint64_t epoch)
{
 rt_base_t level;
 struct mpu_io io={0,read_register,write_register,wait_ms,monotonic_ms};
 rt_err_t result;
 if(!epoch||rt_interrupt_get_nest())return -RT_EINVAL;
 level=rt_hw_interrupt_disable();
 if(ready_claimed){rt_hw_interrupt_enable(level);return epoch==remote_epoch ? -RT_EBUSY : -RT_EINVAL;}
 ready_claimed=1;remote_epoch=epoch;
 rt_hw_interrupt_enable(level);
 result=rockchip_i2c9_resource_ready(rt_tick_from_millisecond(20));
 if(result!=RT_EOK)return initialization_failure(MPU_UNAVAILABLE);
 bus=rt_i2c_bus_device_find("i2c9");if(!bus)return initialization_failure(MPU_UNAVAILABLE);
 previous_tick=rt_tick_get();
 {enum mpu_error error=mpu_init(&sensor,io,0x68);if(error!=MPU_OK)return initialization_failure(error);}
#ifdef MPU_SENSOR_TRACE_V1
 rt_kprintf("MPU_CONFIG epoch=%08x:%08x who=%02x power=%02x accel_fs=%02x gyro_fs=%02x dlpf=%02x divider=%02x config=%08x\n",
     (unsigned)(remote_epoch>>32),(unsigned)remote_epoch,sensor.config.who,sensor.config.power,
     sensor.config.accel,sensor.config.gyro,sensor.config.dlpf,sensor.config.divider,
     (unsigned)sensor.config.config_id);
 rt_kprintf("MPU_RATE odr_target_hz=%u read_target_hz=20 publish_cap_hz=20 raw_trace_limit=100\n",
     (unsigned)sensor.config.odr_hz);
#endif
 result=rt_thread_init(&task,"sensor",sample_loop,0,task_stack,sizeof(task_stack),12,10);
 if(result!=RT_EOK)return initialization_failure(MPU_UNAVAILABLE);
 sampler_running=1;
 result=rt_thread_startup(&task);
 if(result!=RT_EOK){sampler_running=0;return initialization_failure(MPU_UNAVAILABLE);}
 return result;
}
