/* SPDX-License-Identifier: Apache-2.0 */
/* I2C_RESOURCE_PROBE_V1 diagnostic only; no sensor transactions. */
#include <stdint.h>
#include <string.h>
#include <rtthread.h>
#include "hal_base.h"
#include "rpmsg_lite.h"
#include "rpmsg_ns.h"
#include "rpmsg_queue.h"
#include "rpmsg_platform.h"
#include "amp_contract.h"
#include "amp_address.h"
#include "resource_probe.h"

rt_bool_t amp_tick_preflight(void);
void amp_tick_snapshot(const char *phase);

#define AMP_ECHO_ENDPOINT 0x3004U
#define AMP_LINK_WAIT_MS 15000U
#define AMP_TEST_WINDOW_MS 120000U
#define AMP_TEST_REQUEST_LIMIT 128U

static rt_bool_t amp_cache_snapshot(const char *phase)
{
    /* MCU-local ordinary RW CACHE_CTRL; read once, never read SYS_SGRF. */
    const uint32_t ctrl = DCACHE->CACHE_CTRL;
    const uint32_t bypass = !!(ctrl & DCACHE_CACHE_CTRL_CACHE_BYPASS_MASK);
    rt_kprintf("P029 cache %s ctrl=0x%08x bypass=%u\n", phase, ctrl, bypass);
    return bypass ? RT_TRUE : RT_FALSE;
}

static uint32_t probe_read(uint32_t addr)
{
    rt_kprintf("RESOURCE_PROBE_V1 READ_BEGIN addr=0x%08x\n",addr);
    return *(volatile const uint32_t *)(uintptr_t)addr;
}
static void probe_record(uint32_t addr,uint32_t value)
{
    rt_kprintf("RESOURCE_PROBE_V1 VALUE addr=0x%08x value=0x%08x\n",addr,value);
}

void amp_echo_run(void)
{
    struct rpmsg_lite_instance *instance;
    struct rpmsg_lite_endpoint *endpoint;
    rpmsg_queue_handle queue;
    uint32_t src, len;
    rt_tick_t init_start, init_done, start, delay_start, delay_done, timeout_ticks;
    rt_bool_t link_up;
    rt_bool_t first_delay_checkpoint = RT_TRUE;
    unsigned int heartbeat = 0;
    unsigned int count = 0, pong_count = 0;
    char *message;
    uint32_t control_src=0;
    rt_bool_t hello_seen=RT_FALSE, pong_seen=RT_FALSE;
    unsigned int probe_claimed=0;

    rt_kprintf("P029 M0 entered local_fn=0x%08x link=0x%x\n",
               (unsigned int)(uintptr_t)amp_echo_run, AMP_LINK_ID);
    if (!amp_cache_snapshot("entry"))
    {
        rt_kprintf("P029 STOP cache bypass absent; no shared access\n");
        return;
    }
    if (!amp_tick_preflight())
        return;
    init_start = rt_tick_get();
    rt_kprintf("P030 diag remote_init begin tick=%u\n", (unsigned int)init_start);
    instance = rpmsg_lite_remote_init((void *)(uintptr_t)RPMSG_LINUX_MEM_BASE,
                                      AMP_LINK_ID, RL_NO_FLAGS);
    init_done = rt_tick_get();
    rt_kprintf("P030 diag remote_init end tick=%u delta=%u returned=%u\n",
               (unsigned int)init_done, (unsigned int)(init_done - init_start),
               (unsigned int)(instance != RL_NULL));
    if (instance == RL_NULL)
    {
        rt_kprintf("P029 STOP remote init failed\n");
        return;
    }

    start = rt_tick_get();
    timeout_ticks = rt_tick_from_millisecond(AMP_LINK_WAIT_MS);
    rt_kprintf("P030 diag wait begin tick=%u limit_ticks=%u\n",
               (unsigned int)start, (unsigned int)timeout_ticks);
    rt_kprintf("P030 diag link_probe begin tick=%u\n", (unsigned int)rt_tick_get());
    link_up = rpmsg_lite_is_link_up(instance);
    rt_kprintf("P030 diag link_probe end tick=%u up=%u\n",
               (unsigned int)rt_tick_get(), (unsigned int)link_up);
    while (link_up != RT_TRUE)
    {
        if ((rt_tick_t)(rt_tick_get() - start) >= timeout_ticks)
        {
            rt_kprintf("P029 STOP link timeout 15s; echo task exits, MCU stays running\n");
            goto deinit;
        }
        if (first_delay_checkpoint)
        {
            delay_start = rt_tick_get();
            rt_kprintf("P030 diag first_mdelay begin tick=%u duration_ms=1000\n",
                       (unsigned int)delay_start);
        }
        if (first_delay_checkpoint)
            amp_tick_snapshot("before-first-delay");
        rt_thread_mdelay(1000);
        if (first_delay_checkpoint)
        {
            amp_tick_snapshot("after-first-delay");
            delay_done = rt_tick_get();
            rt_kprintf("P030 diag first_mdelay end tick=%u delta=%u\n",
                       (unsigned int)delay_done, (unsigned int)(delay_done - delay_start));
            first_delay_checkpoint = RT_FALSE;
        }
        if (++heartbeat % 5U == 0U && heartbeat <= 15U)
            rt_kprintf("P030 waiting heartbeat=%u tick=%u elapsed_ticks=%u\n",
                       heartbeat, (unsigned int)rt_tick_get(),
                       (unsigned int)(rt_tick_get() - start));
        link_up = rpmsg_lite_is_link_up(instance);
    }
    rt_kprintf("P029 link up\n");
    if (!amp_cache_snapshot("link"))
        goto deinit;
    queue = rpmsg_queue_create(instance);
    if (queue == RL_NULL)
        goto deinit;
    endpoint = rpmsg_lite_create_ept(instance, AMP_ECHO_ENDPOINT, rpmsg_queue_rx_cb, queue);
    if (endpoint == RL_NULL)
        goto destroy_queue;
    if (rpmsg_ns_announce(instance, endpoint, AMP_SERVICE_NAME, RL_NS_CREATE) != RL_SUCCESS)
        goto destroy_endpoint;

    start = rt_tick_get();
    rt_kprintf("I2C_RESOURCE_PROBE_V1 TEST_ONLY window_ms=120000 request_limit=128\n");
    for (; count < AMP_TEST_REQUEST_LIMIT; )
    {
        if ((rt_tick_t)(rt_tick_get() - start) >= rt_tick_from_millisecond(AMP_TEST_WINDOW_MS))
            break;
        len = 0;
        message = RT_NULL;
        if (rpmsg_queue_recv_nocopy(instance, queue, &src, &message, &len, 1000U) != RL_SUCCESS)
            continue;
        count++;
        const uint32_t pa = amp_pool_m0_to_pa((uintptr_t)message);
        if (!pa || pa - AMP_POOL_LINUX_PA < 16U || len > RL_BUFFER_PAYLOAD_SIZE ||
            len > AMP_POOL_SIZE - (pa - AMP_POOL_LINUX_PA))
        {
            rt_kprintf("P029 STOP payload outside expected pool\n");
            /* An invalid pointer cannot safely be handed back to the transport.
             * Its free path dereferences the preceding 16-byte header.
             * Retain ownership until the mandatory cold power cycle. */
            break;
        }
        /* pa is a contract translation of an ACTUAL received pointer, not CON17 readback. */
        rt_kprintf("P029 rx shared_va=0x%08x pa_proposal=0x%08x len=%u\n",
                   (unsigned int)(uintptr_t)message, pa, len);
        char *response = RT_NULL;
        uint32_t response_len = 0;
        rt_bool_t pong = RT_FALSE;
        if (len == 5U && memcmp(message, "HELLO", 5U) == 0)
        {
            if (hello_seen && src!=control_src)
            {
                rt_kprintf("RESOURCE_PROBE_V1 STOP different peer\n");
                break;
            }
            hello_seen=RT_TRUE; control_src=src;
            response = "HELLO_ACK";
            response_len = 9U;
        }
        else if (len == 4U && memcmp(message, "PING", 4U) == 0)
        {
            if (!hello_seen || src!=control_src)
            {
                rt_kprintf("RESOURCE_PROBE_V1 STOP missing handshake\n");
                break;
            }
            pong_seen=RT_TRUE;
            response = "PONG";
            response_len = 4U;
            pong = RT_TRUE;
        }
        else if (len==17U && !memcmp(message,"RESOURCE_PROBE_V1",17U))
        {
            if (!resource_probe_claim(&probe_claimed,hello_seen && pong_seen && src==control_src))
            {
                rt_kprintf("RESOURCE_PROBE_V1 STOP duplicate or unqualified request\n");
                response="RESOURCE_PROBE_STOP_V1"; response_len=22U;
            }
            else
            {
                const struct resource_probe_io io={probe_read,probe_record};
                rt_kprintf("RESOURCE_PROBE_V1 BEGIN reads_only=1 writes=0 i2c_transactions=0\n");
                const int status=resource_probe_once(&io);
                rt_kprintf("RESOURCE_PROBE_V1 END status=%d writes=0 i2c_transactions=0\n",status);
                response=status ? "RESOURCE_PROBE_STOP_V1" : "RESOURCE_PROBE_DONE_V1";
                response_len=22U;
            }
        }
        /* Release each received buffer once; send uses a different owned TX buffer. */
        if (rpmsg_queue_nocopy_free(instance, message) != RL_SUCCESS)
        {
            rt_kprintf("P029 STOP receive buffer release failed\n");
            break;
        }
        if (response == RT_NULL ||
            rpmsg_lite_send(instance, endpoint, src, response, response_len, 100U) != RL_SUCCESS)
        {
            rt_kprintf("P029 STOP unknown request or send failure\n");
            break;
        }
        if (pong)
        {
            pong_count++;
            rt_kprintf("SI PONG sent count=%u\n", pong_count);
            if (!amp_cache_snapshot("after-pong"))
            {
                rt_kprintf("P029 STOP cache bypass lost\n");
                break;
            }
        }
    }
    rt_kprintf("SI echo task complete received=%u pong=%u; no warm restart\n", count, pong_count);
destroy_endpoint:
    rpmsg_lite_destroy_ept(instance, endpoint);
destroy_queue:
    rpmsg_queue_destroy(instance, queue);
deinit:
    rpmsg_lite_deinit(instance);
}
