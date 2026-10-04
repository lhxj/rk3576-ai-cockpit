/* SPDX-License-Identifier: GPL-2.0 */
#ifndef RESOURCE_PROBE_STATE_H
#define RESOURCE_PROBE_STATE_H
static inline int probe_begin(int *state,unsigned long long *deadline,unsigned long long now)
{
    if(*state) return 0;
    *state=1; *deadline=now+3000ULL; return 1;
}
static inline void probe_expire(int *state,unsigned long long deadline,unsigned long long now)
{
    if(*state==1 && now>=deadline) *state=3;
}
static inline int probe_receive(int *state,unsigned long long deadline,unsigned long long now,int done)
{
    if(*state!=1) return 0;
    *state=(now>=deadline || !done) ? 3 : 2;
    return 1;
}
#endif
