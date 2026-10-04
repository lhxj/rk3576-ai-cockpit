/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "resource_probe.h"
#include "resource_probe_state.h"
static unsigned int n,records,mode;
static const uint32_t whitelist[]={0x47200830,0x47200834,0x472003e8,0x47200a30,0x47200a34,0x4ae80000,0x4ae80004,0x4720082c,0x47200a2c,0x4aec0008};
static uint32_t rd(uint32_t addr)
{
 assert(n<10 && addr==whitelist[n]); n++;
 if(addr==0x472003e8) return mode==1 ? 2 : 3;
 if(addr==0x47200a30) return mode==2 ? 1U<<8 : 0;
 if(addr==0x47200a34) return mode==3 ? 1U<<4 : 0;
 if(addr==0x47200830) return mode==4 ? 1U<<8 : 0;
 if(addr==0x47200834) return mode==5 ? 1U<<4 : 0;
 if(addr==0x4ae80000) return mode==6 ? 0x50000 : mode==7 ? 0x60009 : 0x60000;
 if(addr==0x4ae80004) return mode==8 ? UINT32_MAX : 1;
 if(addr==0x4720082c) return mode==9 ? 1U<<12 : 0;
 if(addr==0x47200a2c) return mode==10 ? 1U<<12 : 0;
 return 0;
}
static void rec(uint32_t a,uint32_t v) { (void)v;assert(a==whitelist[records]);records++; }
int main(void)
{
 const struct resource_probe_io io={rd,rec};
 for(mode=0;mode<=10;mode++) {
  n=records=0;int rc=resource_probe_once(&io);
  assert(n==records);
  assert(mode==0 ? (rc==0 && n==10) : mode<=5 ? (rc==1 && n==5) : mode<=8 ? (rc==2 && n==7) : (rc==3 && n==9));
 }
 unsigned int claimed=0;assert(!resource_probe_claim(&claimed,0));assert(!resource_probe_claim(&claimed,1));
 claimed=0;assert(resource_probe_claim(&claimed,1));assert(!resource_probe_claim(&claimed,1));
 int state=0;unsigned long long deadline;
 assert(probe_begin(&state,&deadline,100));assert(deadline==3100);assert(!probe_begin(&state,&deadline,101));
 assert(probe_receive(&state,deadline,3099,1));assert(state==2);assert(!probe_begin(&state,&deadline,3200));
 assert(!probe_receive(&state,deadline,3200,1));
 state=0;assert(probe_begin(&state,&deadline,100));assert(probe_receive(&state,deadline,3100,1));assert(state==3);
 state=0;assert(probe_begin(&state,&deadline,100));probe_expire(&state,deadline,3100);assert(state==3);assert(!probe_receive(&state,deadline,3100,1));
 state=0;assert(probe_begin(&state,&deadline,100));assert(probe_receive(&state,deadline,200,0));assert(state==3);
 puts("RESOURCE_PROBE_ACTUAL_HEADERS_PASS whitelist10 guards10 one-shot deadline/late/stop");return 0;
}
