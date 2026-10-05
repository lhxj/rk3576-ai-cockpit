/* SPDX-License-Identifier: Apache-2.0 */
#include "cockpit/protocol/sensor_v1.h"
#include <string.h>
uint64_t sv_get(const uint8_t *p,unsigned bytes) {
 uint64_t value=0;unsigned i;
 for(i=0;i<bytes;i++)value=(value<<8)|p[i];
 return value;
}
int16_t sv_get_signed16(const uint8_t *p) {
 uint32_t value=(uint32_t)sv_get(p,2);
 return (int16_t)(value>=32768 ? (int32_t)value-65536 : (int32_t)value);
}
void sv_put(uint8_t *p,uint64_t value,unsigned bytes) {
 unsigned i;
 for(i=0;i<bytes;i++)p[i]=(uint8_t)(value>>((bytes-1-i)*8));
}
size_t sv_payload_size(uint16_t type) {
 switch(type) {
 case SV_HELLO:return 16;
 case SV_STATUS:return 72;
 case SV_SUBSCRIBE:return 28;
 case SV_UNSUBSCRIBE:return 20;
 case SV_SAMPLE:return 72;
 case SV_ACK:case SV_RESULT:case SV_ERROR:return 24;
 default:return 0;
 }
}
int sv_validate(const struct sv_frame *f) {
 const uint8_t *p;
 if(!f || !sv_payload_size(f->type) || f->size!=sv_payload_size(f->type))return SV_MALFORMED;
 p=f->payload;
 if(sv_get(p,2)!=1 || sv_get(p+2,2)!=1)return SV_VERSION;
 if(f->type==SV_HELLO) {
  uint64_t flags=sv_get(p+12,2);
  if((flags!=1 && flags!=2) || sv_get(p+14,2))return SV_MALFORMED;
  if((flags==1 && sv_get(p+4,8)) || (flags==2 && !sv_get(p+4,8)))return SV_MALFORMED;
 } else if(f->type==SV_SUBSCRIBE) {
  if(!sv_get(p+4,8)||!sv_get(p+12,8)||sv_get(p+20,2)!=20||sv_get(p+22,2)<500||sv_get(p+22,2)>5000||sv_get(p+24,4))return SV_MALFORMED;
 } else if(f->type==SV_UNSUBSCRIBE) {
  if(!sv_get(p+4,8)||!sv_get(p+12,8))return SV_MALFORMED;
 } else if(f->type==SV_SAMPLE) {
  if(!sv_get(p+4,8)||!sv_get(p+12,8)||!sv_get(p+20,8)||!sv_get(p+28,8)||p[44]!=1||p[45]!=1||sv_get(p+46,2)||p[62]||p[63]||p[64]!=3||p[65]!=49||p[66]!=1||p[67]||sv_get(p+68,4)!=SV_CONFIG_ID)return SV_MALFORMED;
 } else if(f->type==SV_STATUS) {
  if(p[48]>2||p[49]>1||sv_get(p+50,2)>6||p[52]||p[53]||p[54]!=3||p[55]!=49||p[56]!=1||p[57]||sv_get(p+58,2)!=20||sv_get(p+60,4)!=SV_CONFIG_ID)return SV_MALFORMED;
 } else {
  uint64_t type=sv_get(p+22,2);
  if(sv_get(p+20,2)>SV_HARDWARE || (type!=SV_HELLO&&type!=SV_SUBSCRIBE&&type!=SV_UNSUBSCRIBE))return SV_MALFORMED;
 }
 return SV_OK;
}
int sv_encode(const struct sv_frame*f,uint8_t *wire,size_t capacity,size_t *length) {
 int error=sv_validate(f);
 if(error)return error;
 if(!wire||!length||capacity<SV_HEADER_SIZE+f->size)return SV_LIMIT;
 sv_put(wire,0x56414931,4);sv_put(wire+4,1,2);sv_put(wire+6,f->type,2);sv_put(wire+8,f->size,4);
 sv_put(wire+12,f->request,8);sv_put(wire+20,f->session,8);sv_put(wire+28,f->core_epoch,8);sv_put(wire+36,0,8);
 memcpy(wire+SV_HEADER_SIZE,f->payload,f->size);*length=SV_HEADER_SIZE+f->size;return SV_OK;
}
int sv_decode(const uint8_t*wire,size_t length,struct sv_frame*f) {
 struct sv_frame parsed;size_t payload;
 if(!wire||!f||length<SV_HEADER_SIZE||length>SV_MAX_WIRE)return SV_MALFORMED;
 if(sv_get(wire,4)!=0x56414931)return SV_MALFORMED;
 if(sv_get(wire+4,2)!=1)return SV_VERSION;
 payload=(size_t)sv_get(wire+8,4);
 if(payload!=length-SV_HEADER_SIZE||payload>sizeof(parsed.payload)||sv_get(wire+36,8))return SV_MALFORMED;
 memset(&parsed,0,sizeof(parsed));parsed.type=(uint16_t)sv_get(wire+6,2);parsed.size=payload;
 parsed.request=sv_get(wire+12,8);parsed.session=sv_get(wire+20,8);parsed.core_epoch=sv_get(wire+28,8);
 memcpy(parsed.payload,wire+SV_HEADER_SIZE,payload);
 {int error=sv_validate(&parsed);if(error)return error;}
 *f=parsed;return SV_OK;
}
