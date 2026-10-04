/* SPDX-License-Identifier: Apache-2.0 */
#ifndef I2C_RESOURCE_PROBE_H
#define I2C_RESOURCE_PROBE_H
#include <stdint.h>
/* Fixed RK3576 BUS_MCU aliases; pure reads, no peripheral transactions. */
#define PROBE_CRU 0x47200000U
#define PROBE_I2C9 0x4ae80000U
#define PROBE_INTMUX 0x4aec0000U
static int resource_probe_claim(unsigned int *claimed,int qualified)
{
    if (*claimed) return 0;
    *claimed=1;
    return qualified;
}
struct resource_probe_io { uint32_t (*read)(uint32_t); void (*record)(uint32_t,uint32_t); };
/* Return 0 only after the complete whitelist; partial reads never mean permission PASS. */
static int resource_probe_once(const struct resource_probe_io *io)
{
    uint32_t pclk,clk,parent,prst,rst,con,div,gate,reset;
#define READ_TO(v,a) do { (v)=io->read(a); io->record(a,v); } while (0)
    READ_TO(pclk,PROBE_CRU+0x830U);
    READ_TO(clk,PROBE_CRU+0x834U);
    READ_TO(parent,PROBE_CRU+0x3e8U);
    READ_TO(prst,PROBE_CRU+0xa30U);
    READ_TO(rst,PROBE_CRU+0xa34U);
    if ((pclk & (1U<<8)) || (clk & (1U<<4)) || (parent & 3U)!=3U ||
        (prst & (1U<<8)) || (rst & (1U<<4))) return 1;
    READ_TO(con,PROBE_I2C9);
    READ_TO(div,PROBE_I2C9+4U);
    if ((con>>16)!=6U || (con & 0x19U) || div==UINT32_MAX) return 2;
    READ_TO(gate,PROBE_CRU+0x82cU);
    READ_TO(reset,PROBE_CRU+0xa2cU);
    if ((gate & (1U<<12)) || (reset & (1U<<12))) return 3;
    /* Read-only INTMUX enable bank2. No IRQ install/mask/register writes. */
    READ_TO(gate,PROBE_INTMUX+8U);
#undef READ_TO
    return 0;
}
#endif
