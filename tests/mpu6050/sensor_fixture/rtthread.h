#ifndef FIXTURE_RTTHREAD_H
#define FIXTURE_RTTHREAD_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t rt_tick_t,rt_uint32_t;typedef uint16_t rt_uint16_t;typedef uint8_t rt_uint8_t;typedef int32_t rt_int32_t;typedef int rt_err_t,rt_base_t;typedef size_t rt_size_t;
#define RT_EOK 0
#define RT_EINVAL 22
#define RT_EBUSY 16
#define RT_ETIMEOUT 2
#define RT_ENOSYS 38
#define RT_ERROR 1
#ifndef RT_TICK_PER_SECOND
#define RT_TICK_PER_SECOND 1000
#endif
struct rt_thread { int value; };
rt_tick_t rt_tick_get(void);rt_tick_t rt_tick_from_millisecond(int);int rt_interrupt_get_nest(void);void rt_thread_mdelay(int);int rt_kprintf(const char*,...);
rt_err_t rt_thread_init(struct rt_thread*,const char*,void(*)(void*),void*,void*,rt_uint32_t,rt_uint8_t,rt_uint32_t);rt_err_t rt_thread_startup(struct rt_thread*);
#endif
