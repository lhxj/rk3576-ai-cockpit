/* SPDX-License-Identifier: Apache-2.0 */
#ifndef COCKPIT_SENSOR_V1_H
#define COCKPIT_SENSOR_V1_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SV_HEADER_SIZE 44u
#define SV_MAX_WIRE 256u
#define SV_SAMPLE_SIZE 72u
#define SV_CONFIG_ID 0x00010331u
enum sv_type { SV_ACK=19, SV_RESULT=20, SV_ERROR=21, SV_HELLO=0x100, SV_STATUS=0x101, SV_SUBSCRIBE=0x102, SV_UNSUBSCRIBE=0x103, SV_SAMPLE=0x104 };
enum sv_error { SV_OK=0, SV_MALFORMED=1, SV_VERSION=2, SV_STALE_EPOCH=3, SV_OWNER=4, SV_STATE=5, SV_LIMIT=6, SV_DUPLICATE=7, SV_HARDWARE=8 };
struct sv_frame { uint16_t type; uint64_t request,session,core_epoch; size_t size; uint8_t payload[SV_MAX_WIRE-SV_HEADER_SIZE]; };
int16_t sv_get_signed16(const uint8_t*);
uint64_t sv_get(const uint8_t*,unsigned);
void sv_put(uint8_t*,uint64_t,unsigned);
size_t sv_payload_size(uint16_t);
int sv_validate(const struct sv_frame*);
int sv_encode(const struct sv_frame*,uint8_t*,size_t,size_t*);
int sv_decode(const uint8_t*,size_t,struct sv_frame*);
#ifdef __cplusplus
}
#endif
#endif
