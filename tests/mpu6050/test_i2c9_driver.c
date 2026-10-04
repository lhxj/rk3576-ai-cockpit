/* Compile the actual patched vendor driver, not a reimplemented model. */
#include "fake_i2c9_platform.h"
#include "drv_i2c.c"
static void reset(void){i2c9_owner_state=I2C9_WAIT_RESOURCE;i2c9_init_result=-RT_EBUSY;i2c9.transfer_active=false;reset_calls=reset_fail_on=intmux_gate_calls=intmux_gate_result=0;hardware_calls=gates=init_calls=register_calls=force_stop=close_calls=irq_calls=0;init_result=register_result=irq_result=wait_result=0;clock_rate=24000000;immediate_irq=duplicate_irq=reentrant_init=false;ticks=wait_advance=waited=configure_advance=0;irq_masked=true;stale_pending=false;}
int main(void){
 uint8_t reg=0x75,data[14]={0};struct rt_i2c_msg m[2]={{0x68,0,1,&reg},{0x68,RT_I2C_RD,14,data}};
 rt_tick_t budget=rt_tick_from_millisecond(20);
 reset();assert(rockchip_rt_hw_i2c_init()==0&&hardware_calls==0);
 assert((int)rockchip_i2c_xfer(&i2c9.bus,m,2)==-RT_EBUSY&&hardware_calls==0);
 assert(rockchip_i2c9_resource_ready(0)==-RT_EINVAL&&hardware_calls==0);
 assert(rockchip_i2c9_resource_ready(budget+1)==-RT_EINVAL&&hardware_calls==0);
 nested=1;assert(rockchip_i2c9_resource_ready(budget)==-RT_EINVAL&&hardware_calls==0);nested=0;
 reentrant_init=true;assert(rockchip_i2c9_resource_ready(budget)==0);assert(reentrant_result==-RT_EBUSY&&reset_calls==2);
 int before=hardware_calls;assert(rockchip_i2c9_resource_ready(budget)==0&&hardware_calls==before&&init_calls==1&&register_calls==1);
 stale_pending=true;immediate_irq=duplicate_irq=true;assert(rockchip_i2c_xfer(&i2c9.bus,m,2)==2);assert(waited==budget&&irq_calls==1&&gates==0);
 before=irq_calls;handler(161,handler_param);assert(irq_calls==before);
 immediate_irq=false;wait_result=-RT_ETIMEOUT;assert((int)rockchip_i2c_xfer(&i2c9.bus,m,2)==-RT_ETIMEOUT&&force_stop==1);
 wait_result=0;irq_result=-6;assert((int)rockchip_i2c_xfer(&i2c9.bus,m,2)==-6);
 irq_result=0;assert(rockchip_i2c_xfer(&i2c9.bus,m,2)==2);assert(!i2c9.transfer_active&&gates==0);
 configure_advance=1;assert(rockchip_i2c_xfer(&i2c9.bus,m,2)==2&&waited==budget-1);configure_advance=budget;before=irq_calls;assert((int)rockchip_i2c_xfer(&i2c9.bus,m,2)==-RT_ETIMEOUT&&irq_calls==before);configure_advance=0;
 /* Two separate messages share one 20ms budget, even across tick rollover. */
 m[1].flags=0;ticks=UINT32_MAX-1;wait_advance=budget;assert((int)rockchip_i2c_xfer(&i2c9.bus,m,2)==-RT_ETIMEOUT);
 reset();init_result=-1;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&register_calls==0);before=hardware_calls;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&hardware_calls==before);
 reset();register_result=-7;assert(rockchip_i2c9_resource_ready(budget)==-7);before=hardware_calls;assert(rockchip_i2c9_resource_ready(budget)==-7&&hardware_calls==before);
 reset();intmux_gate_result=-1;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&reset_calls==0&&init_calls==0);before=hardware_calls;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&hardware_calls==before);
 reset();reset_fail_on=1;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&reset_calls==1&&init_calls==0);before=hardware_calls;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&hardware_calls==before);
 reset();reset_fail_on=2;assert(rockchip_i2c9_resource_ready(budget)==-RT_ERROR&&reset_calls==2&&init_calls==0);
 reset();clock_rate=12000000;assert(rockchip_i2c9_resource_ready(budget)==-RT_EINVAL&&init_calls==0);
 printf("PASS actual I2C9 driver: tick_hz=%d budget=%u ticks; deferred/held-clock/failure/IRQ/timeout regressions\n",RT_TICK_PER_SECOND,budget);return 0;
}
