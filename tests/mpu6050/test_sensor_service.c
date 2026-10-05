/* SPDX-License-Identifier: Apache-2.0 */
#include "sensor_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct transport {struct sv_frame last; unsigned sends,init;int fail,init_error;};
static int send_fake(void*c,uint32_t peer,const uint8_t*w,size_t n){struct transport*t=c;assert(peer==7||peer==8);t->sends++;assert(sv_decode(w,n,&t->last)==SV_OK);return t->fail;}
static int init_fake(void*c,uint64_t epoch){struct transport*t=c;assert(epoch==99);t->init++;return t->init_error;}
static struct sv_frame request(uint16_t type,uint64_t id,uint64_t epoch){struct sv_frame f={0};f.type=type;f.size=sv_payload_size(type);f.request=id;f.session=11;f.core_epoch=22;sv_put(f.payload,1,2);sv_put(f.payload+2,1,2);sv_put(f.payload+4,epoch,8);return f;}
static int accept(struct sensor_service*s,uint32_t peer,struct sv_frame*f,uint64_t now){uint8_t bytes[256];size_t n;assert(sv_encode(f,bytes,sizeof(bytes),&n)==SV_OK);return sensor_service_request(s,peer,bytes,n,now);}
int main(void){
 struct transport t={0};struct sensor_service s;struct sensor_service_io io={&t,send_fake,init_fake};
 struct sv_frame f=request(SV_HELLO,1,99),out;uint8_t bytes[257];size_t n;
 sv_put(f.payload+12,2,2);assert(sv_encode(&f,bytes,256,&n)==SV_OK&&n==60);
 const uint8_t golden_prefix[]={0x56,0x41,0x49,0x31,0,1,1,0,0,0,0,16};assert(!memcmp(bytes,golden_prefix,sizeof(golden_prefix)));
 assert(bytes[19]==1&&bytes[27]==11&&bytes[35]==22&&bytes[51]==0&&bytes[55]==99&&bytes[57]==2);
 assert(sv_decode(bytes,n,&out)==SV_OK&&out.request==1&&out.core_epoch==22);
 for(size_t i=0;i<n;i++)assert(sv_decode(bytes,i,&out)!=SV_OK);
 assert(sv_decode(bytes,257,&out)!=SV_OK);
 bytes[4]=1;assert(sv_decode(bytes,n,&out)==SV_VERSION);bytes[4]=0;
 bytes[7]=0xff;assert(sv_decode(bytes,n,&out)==SV_MALFORMED);bytes[7]=0;
 bytes[59]=1;assert(sv_decode(bytes,n,&out)==SV_MALFORMED);bytes[59]=0;
 bytes[36]=1;assert(sv_decode(bytes,n,&out)==SV_MALFORMED);bytes[36]=0;
 sensor_service_init(&s,io,0);assert(accept(&s,7,&f,0)==SV_OK&&s.epoch==99&&t.init==1);
 assert(accept(&s,7,&f,1)==SV_OK&&t.init==1);
 sv_put(f.payload+4,100,8);assert(accept(&s,7,&f,2)==SV_STALE_EPOCH&&s.epoch==99);
 f=request(SV_HELLO,2,0);sv_put(f.payload+12,1,2);assert(accept(&s,8,&f,3)==SV_OK&&s.peer==7&&s.epoch==99);
 f=request(SV_SUBSCRIBE,3,99);sv_put(f.payload+12,123,8);sv_put(f.payload+20,20,2);sv_put(f.payload+22,500,2);
 assert(accept(&s,8,&f,4)==SV_OWNER);assert(accept(&s,7,&f,4)==SV_OK&&s.subscription==123);
 assert(accept(&s,7,&f,5)==SV_DUPLICATE&&s.lease_end==504);
 struct mpu_sample_state sample={0};sample.valid=1;sample.sample_seq=1;sample.last_valid_ms=10;sample.last.accel[0]=-32768;
 sensor_service_offer(&s,&sample);sensor_service_poll(&s,10);assert(t.last.type==SV_SAMPLE&&sv_get(t.last.payload+48,2)==0x8000&&sv_get_signed16(t.last.payload+48)==-32768&&s.publish_seq==1);
 struct sv_frame bad_sample=t.last;
 unsigned bad_offsets[]={44,45,46,62,63,64,65,66,67,68};
 for(unsigned k=0;k<sizeof(bad_offsets)/sizeof(bad_offsets[0]);k++) {struct sv_frame changed=bad_sample;changed.payload[bad_offsets[k]]^=1;assert(sv_validate(&changed)==SV_MALFORMED);}
 unsigned sends=t.sends;f.request=30;assert(accept(&s,7,&f,11)==SV_OK);sends=t.sends;sample.sample_seq=2;sensor_service_offer(&s,&sample);sensor_service_poll(&s,59);assert(t.sends==sends);
 sample.sample_seq=3;sensor_service_offer(&s,&sample);assert(s.overwrites==1);
 t.fail=1;sensor_service_poll(&s,60);assert(s.publish_seq==2&&s.send_failures==1&&!s.pending);sensor_service_poll(&s,110);assert(s.publish_seq==2);t.fail=0;
 sample.valid=0;sample.error=MPU_TIMEOUT;sample.errors=1;sensor_service_offer(&s,&sample);assert(!s.valid&&s.sample_time==10&&!s.pending);
 sample.valid=1;sample.error=0;sample.sample_seq=4;sample.last_valid_ms=120;sensor_service_offer(&s,&sample);sensor_service_poll(&s,120);assert(s.publish_seq==3);
 sensor_service_poll(&s,511);assert(!s.subscription);sample.sample_seq=5;sensor_service_offer(&s,&sample);sensor_service_poll(&s,512);assert(s.publish_seq==3);
 f.request=4;assert(accept(&s,7,&f,520)==SV_OK);
 struct sv_frame unsub=request(SV_UNSUBSCRIBE,5,99);sv_put(unsub.payload+12,123,8);assert(accept(&s,7,&unsub,521)==SV_OK&&!s.subscription);
 f=request(SV_HELLO,6,99);sv_put(f.payload+12,2,2);f.session=12;assert(accept(&s,8,&f,530)==SV_OK&&s.peer==8&&t.init==1);
 struct sv_frame invalid=t.last;
 if(invalid.type==SV_STATUS){invalid.payload[57]=1;assert(sv_validate(&invalid)==SV_MALFORMED);}
 sensor_service_poll(&s,900000);assert(s.stopped);assert(accept(&s,8,&f,900001)==SV_STATE);
 struct sensor_service failed;
 t.init_error=MPU_WRONG_ID;sensor_service_init(&failed,io,0);
 f=request(SV_HELLO,1,99);sv_put(f.payload+12,2,2);
 unsigned init_count=t.init;assert(accept(&failed,7,&f,0)==SV_OK&&failed.ready_failed&&failed.sample_error==MPU_WRONG_ID);
 assert(t.last.payload[48]==2&&sv_get(t.last.payload+50,2)==MPU_WRONG_ID);
 assert(accept(&failed,7,&f,1)==SV_OK&&t.init==init_count+1);
 struct sv_frame bad=request(SV_SUBSCRIBE,2,99);sv_put(bad.payload+12,1,8);sv_put(bad.payload+20,21,2);sv_put(bad.payload+22,500,2);assert(sv_validate(&bad)==SV_MALFORMED);
 sv_put(bad.payload+20,20,2);sv_put(bad.payload+22,499,2);assert(sv_validate(&bad)==SV_MALFORMED);
 sv_put(bad.payload+22,5001,2);assert(sv_validate(&bad)==SV_MALFORMED);
 puts("SENSOR_CODEC_SERVICE_FAKE_PASS");return 0;
}
