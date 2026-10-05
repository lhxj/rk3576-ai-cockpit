#ifndef FIXTURE_RPMSG_H
#define FIXTURE_RPMSG_H
#include <stdint.h>
#define RL_RELEASE 0
#define RL_HOLD 1
#define RL_DONT_BLOCK 0
#define RL_SUCCESS 0
#define RL_BUFFER_PAYLOAD_SIZE 496
struct rpmsg_lite_instance {int value;};
struct rpmsg_lite_endpoint {uint32_t addr;};
struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(struct rpmsg_lite_instance*,uint32_t,int32_t(*)(void*,uint32_t,uint32_t,void*),void*);
int rpmsg_lite_destroy_ept(struct rpmsg_lite_instance*,struct rpmsg_lite_endpoint*);
int rpmsg_lite_send(struct rpmsg_lite_instance*,struct rpmsg_lite_endpoint*,uint32_t,char*,uint32_t,uint32_t);
#endif
