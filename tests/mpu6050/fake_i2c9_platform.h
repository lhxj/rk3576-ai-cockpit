#ifndef FAKE_PLATFORM_H
#define FAKE_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#define RT_USING_I2C
#define RT_USING_I2C9
#define HAL_CRU_MODULE_ENABLED
#define MPU_SENSOR_V1_I2C9_OWNERSHIP
#define RT_NULL NULL
#define RT_WEAK
#define INIT_PREV_EXPORT(x)
#define RT_EOK 0
#define RT_ERROR 1
#define RT_EBUSY 16
#define RT_EINVAL 22
#define RT_ETIMEOUT 110
#define RT_I2C_RD 1
#define HAL_OK 0
#define HAL_BUSY (-16)
#define I2C_IT 1
#define I2C_POLL 0
#define REG_CON_MOD_REGISTER_TX 1
#define REG_CON_MOD_TX 0
#define HAL_I2C_REG_MRXADDR_VALID(x) (1U << (24+(x)))
typedef int rt_err_t;
typedef uintptr_t rt_base_t;
typedef uint32_t rt_tick_t;
typedef uint32_t rt_uint32_t;
typedef size_t rt_size_t;
typedef void (*rt_isr_handler_t)(int,void*);
typedef enum { I2C_100K=100000 } eI2C_BusSpeed;
struct I2C_REG { int dummy; };
struct I2C_HANDLE { int dummy; };
struct HAL_I2C_DEV { struct I2C_REG *pReg; int irqNum,pclkGateID,clkGateID,clkID; };
struct rt_completion { bool done; };
struct rt_device { void *user_data; };
struct rt_i2c_msg { uint16_t addr,flags,len; uint8_t *buf; };
struct rt_i2c_bus_device;
struct rt_i2c_bus_device_ops { rt_size_t (*master_xfer)(struct rt_i2c_bus_device*,struct rt_i2c_msg*,rt_uint32_t); };
struct rt_i2c_bus_device { struct rt_device parent; void *priv; struct rt_i2c_bus_device_ops *ops; rt_tick_t timeout; };
struct rockchip_i2c_config { struct I2C_REG *id; eI2C_BusSpeed speed; };
static struct I2C_REG reg9;
static const struct HAL_I2C_DEV g_i2c9Dev={&reg9,161,200,212,130};
static int hardware_calls, gates, mux7, nested, init_calls, register_calls, force_stop, close_calls, irq_calls;
static int init_result, register_result, irq_result, wait_result, clock_rate=24000000;
static bool immediate_irq, duplicate_irq, irq_masked=true, stale_pending;
static rt_tick_t configure_advance;
static rt_tick_t ticks,waited,wait_advance;
static rt_err_t reentrant_result;
static bool reentrant_init;
rt_err_t rockchip_i2c9_resource_ready(rt_tick_t);
static rt_isr_handler_t handler;
static void *handler_param;
static void rt_interrupt_enter(void){nested++;}
static void rt_interrupt_leave(void){nested--;}
static int rt_interrupt_get_nest(void){return nested;}
static rt_base_t rt_hw_interrupt_disable(void){return 0;}
static void rt_hw_interrupt_enable(rt_base_t l){(void)l;}
static void rt_hw_interrupt_mask(int irq){assert(irq==161);irq_masked=true;hardware_calls++;}
static void rt_hw_interrupt_umask(int irq){assert(irq==161);assert(!stale_pending);irq_masked=false;hardware_calls++;}
static void rt_hw_interrupt_install(int irq,rt_isr_handler_t h,void*p,const char*n){assert(irq==161);(void)n;handler=h;handler_param=p;hardware_calls++;}
static rt_tick_t rt_tick_get(void){return ticks;}
static rt_tick_t rt_tick_from_millisecond(int ms){return (ms*RT_TICK_PER_SECOND+999)/1000;}
static void rt_thread_delay(int n){ticks+=n;}
static void rt_completion_init(struct rt_completion*c){c->done=false;}
static void rt_completion_done(struct rt_completion*c){c->done=true;}
static int rt_completion_wait(struct rt_completion*c,rt_tick_t n){waited=n;ticks+=wait_advance;if(!wait_result&&!c->done)handler(161,handler_param);return wait_result;}
static void clk_enable_by_id(int id){(void)id;gates++;hardware_calls++;}
static void clk_disable_by_id(int id){(void)id;gates++;hardware_calls++;}
static unsigned clk_get_rate(int id){(void)id;hardware_calls++;return clock_rate;}
static int HAL_I2C_Init(struct I2C_HANDLE*h,struct I2C_REG*r,unsigned f,eI2C_BusSpeed s){(void)h;assert(r==&reg9&&f==24000000&&s==I2C_100K);hardware_calls++;init_calls++;if(reentrant_init)reentrant_result=rockchip_i2c9_resource_ready(rt_tick_from_millisecond(20));return init_result;}
static int rt_i2c_bus_device_register(struct rt_i2c_bus_device*b,const char*n){(void)b;assert(n[3]=='9');register_calls++;return register_result;}
static int HAL_I2C_IRQHandler(struct I2C_HANDLE*h){(void)h;hardware_calls++;irq_calls++;return irq_result;}
static void HAL_I2C_ConfigureMode(struct I2C_HANDLE*h,int m,unsigned a,unsigned r){(void)h;(void)m;(void)a;(void)r;assert(irq_masked);ticks+=configure_advance;hardware_calls++;}
static void HAL_I2C_SetupMsg(struct I2C_HANDLE*h,unsigned a,uint8_t*b,unsigned n,unsigned f){(void)h;(void)a;(void)b;(void)n;(void)f;hardware_calls++;}
static int HAL_I2C_Transfer(struct I2C_HANDLE*h,int t,bool last){(void)h;(void)t;(void)last;assert(irq_masked);stale_pending=false;hardware_calls++;if(immediate_irq){handler(161,handler_param);if(duplicate_irq){int before=irq_calls;irq_result=-6;handler(161,handler_param);assert(irq_calls==before);irq_result=0;}}return 0;}
static void HAL_I2C_ForceStop(struct I2C_HANDLE*h){(void)h;force_stop++;hardware_calls++;}
static void HAL_I2C_Close(struct I2C_HANDLE*h){(void)h;close_calls++;hardware_calls++;}
#define rt_kprintf(...) ((void)0)
#endif
