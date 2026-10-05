/* SPDX-License-Identifier: Apache-2.0 */
#include "sensor_service.h"
#include <string.h>
static void base_frame(struct sensor_service*s,struct sv_frame*f,uint16_t type,uint64_t request) {
 memset(f,0,sizeof(*f));f->type=type;f->size=sv_payload_size(type);
 f->request=request;f->session=s->session;f->core_epoch=s->core_epoch;
 sv_put(f->payload,1,2);sv_put(f->payload+2,1,2);sv_put(f->payload+4,s->epoch,8);
}
static int send_frame(struct sensor_service*s,uint32_t peer,struct sv_frame*f) {
 uint8_t wire[SV_MAX_WIRE];size_t length;
 if(sv_encode(f,wire,sizeof(wire),&length)!=SV_OK)return SV_MALFORMED;
 if(s->io.send(s->io.context,peer,wire,length)){s->send_failures++;return SV_LIMIT;}
 return SV_OK;
}
static void expire(struct sensor_service*s,uint64_t now) {
 if(!s->stopped && now-s->start_ms>=SENSOR_RUN_WINDOW_MS){s->stopped=1;s->subscription=0;s->pending=0;s->valid=0;s->sample_error=MPU_UNAVAILABLE;s->status_dirty=1;}
 if(s->subscription && now>=s->lease_end){s->subscription=0;s->pending=0;}
}
static int status(struct sensor_service*s,uint32_t peer,const struct sv_frame*request) {
 struct sv_frame f;base_frame(s,&f,SV_STATUS,request->request);
 f.session=request->session;f.core_epoch=request->core_epoch;
 sv_put(f.payload+12,s->subscription,8);sv_put(f.payload+20,s->sample_seq,8);sv_put(f.payload+28,s->publish_seq,8);
 sv_put(f.payload+36,s->errors,4);sv_put(f.payload+40,s->overwrites,4);sv_put(f.payload+44,s->send_failures,4);
 f.payload[48]=s->ready_failed ? 2:s->initialized;f.payload[49]=s->valid;sv_put(f.payload+50,s->sample_error,2);
 f.payload[54]=3;f.payload[55]=49;f.payload[56]=1;sv_put(f.payload+58,20,2);sv_put(f.payload+60,SV_CONFIG_ID,4);
 sv_put(f.payload+64,s->protocol_errors,4);sv_put(f.payload+68,s->control_drops,4);
 return send_frame(s,peer,&f);
}
static int reply(struct sensor_service*s,const struct sv_frame*request,int error) {
 struct sv_frame f;base_frame(s,&f,error ? SV_ERROR:SV_RESULT,request->request);
 sv_put(f.payload+12,s->subscription,8);sv_put(f.payload+20,error,2);sv_put(f.payload+22,request->type,2);
 (void)send_frame(s,s->peer,&f);return error;
}
void sensor_service_init(struct sensor_service*s,struct sensor_service_io io,uint64_t now) {
 memset(s,0,sizeof(*s));s->io=io;s->start_ms=now;
}
int sensor_service_request(struct sensor_service*s,uint32_t peer,const uint8_t*wire,size_t length,uint64_t now) {
 struct sv_frame f;unsigned i;int error;
 if(!s||!s->io.send||!s->io.initialize)return SV_STATE;
 expire(s,now);
 error=sv_decode(wire,length,&f);if(error){s->protocol_errors++;return error;}
 if(s->stopped)return SV_STATE;
 if(!f.request||!f.session||!f.core_epoch)return SV_OWNER;
 if(f.type==SV_HELLO) {
  uint64_t nonce=sv_get(f.payload+4,8),flags=sv_get(f.payload+12,2);
  if(flags==1)return status(s,peer,&f); /* query never mutates bound epoch/owner */
  if(s->epoch && nonce!=s->epoch)return SV_STALE_EPOCH;
  if(s->subscription && (peer!=s->peer||f.session!=s->session||f.core_epoch!=s->core_epoch))return SV_OWNER;
  if(!s->epoch)s->epoch=nonce;
  if(peer!=s->peer||f.session!=s->session||f.core_epoch!=s->core_epoch){memset(s->recent,0,sizeof(s->recent));s->recent_next=0;}
  s->peer=peer;s->session=f.session;s->core_epoch=f.core_epoch;
  if(!s->initialized&&!s->ready_failed) {
   int initialization_error=s->io.initialize(s->io.context,s->epoch);
   if(initialization_error){s->ready_failed=1;s->valid=0;s->errors++;s->sample_error=initialization_error<=MPU_UNAVAILABLE && initialization_error>0 ? (uint16_t)initialization_error:MPU_UNAVAILABLE;s->status_dirty=1;}
   else s->initialized=1;
  }
  return status(s,peer,&f);
 }
 if(f.type!=SV_SUBSCRIBE&&f.type!=SV_UNSUBSCRIBE)return SV_STATE;
 if(peer!=s->peer||f.session!=s->session||f.core_epoch!=s->core_epoch)return SV_OWNER;
 if(sv_get(f.payload+4,8)!=s->epoch)return reply(s,&f,SV_STALE_EPOCH);
 if(!s->initialized)return reply(s,&f,SV_HARDWARE);
 for(i=0;i<8;i++)if(s->recent[i]==f.request)return reply(s,&f,SV_DUPLICATE);
 s->recent[s->recent_next++%8]=f.request;
 if(f.type==SV_SUBSCRIBE) {
  uint64_t subscription=sv_get(f.payload+12,8);
  if(s->subscription && subscription!=s->subscription)return reply(s,&f,SV_STATE);
  s->subscription=subscription;s->lease_end=now+sv_get(f.payload+22,2);
 } else {
  if(s->subscription && sv_get(f.payload+12,8)!=s->subscription)return reply(s,&f,SV_STATE);
  s->subscription=0;s->pending=0;
 }
 return reply(s,&f,SV_OK);
}
void sensor_service_offer(struct sensor_service*s,const struct mpu_sample_state*sample) {
 if(!s||!sample||s->stopped)return;
 if(s->valid!=sample->valid||s->sample_error!=sample->error)s->status_dirty=1;
 s->valid=sample->valid;s->sample_error=sample->error;s->errors=sample->errors;
 if(!sample->valid){s->pending=0;return;} /* never refresh last valid data/time after failure */
 if(sample->sample_seq<=s->sample_seq)return;
 if(s->pending)s->overwrites++;
 s->sample_seq=sample->sample_seq;s->sample_time=sample->last_valid_ms;s->raw=sample->last;
 s->pending=s->subscription ? 1:0;
}
void sensor_service_poll(struct sensor_service*s,uint64_t now) {
 struct sv_frame f;unsigned i;
 expire(s,now);
 if(s->epoch&&s->status_dirty&&now>=s->next_status) {
  struct sv_frame notification;memset(&notification,0,sizeof(notification));notification.session=s->session;notification.core_epoch=s->core_epoch;
  s->status_dirty=0;s->next_status=now+1000;(void)status(s,s->peer,&notification);
 }
 if(!s->subscription||!s->pending||now<s->next_publish)return;
 /* Consume this latest item even if send fails; a slow peer cannot induce retry. */
 s->pending=0;s->publish_seq++;s->next_publish=now+50;
 base_frame(s,&f,SV_SAMPLE,0);sv_put(f.payload+12,s->subscription,8);sv_put(f.payload+20,s->sample_seq,8);sv_put(f.payload+28,s->publish_seq,8);sv_put(f.payload+36,s->sample_time,8);
 f.payload[44]=1;f.payload[45]=1;
 for(i=0;i<3;i++)sv_put(f.payload+48+i*2,(uint16_t)s->raw.accel[i],2);
 sv_put(f.payload+54,(uint16_t)s->raw.temperature,2);
 for(i=0;i<3;i++)sv_put(f.payload+56+i*2,(uint16_t)s->raw.gyro[i],2);
 f.payload[64]=3;f.payload[65]=49;f.payload[66]=1;sv_put(f.payload+68,SV_CONFIG_ID,4);
 (void)send_frame(s,s->peer,&f);
}
