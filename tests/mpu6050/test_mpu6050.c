/* SPDX-License-Identifier: Apache-2.0 */
#include "mpu6050.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct fake { uint8_t registers[128]; unsigned reads,writes,waits; int error,short_read,reset_stuck,bad_reg,write_error,short_write; uint64_t clock; };
static int read_fake(void *ctx,uint8_t addr,uint8_t reg,uint8_t *out,size_t len) {
 struct fake *f=ctx;assert(addr==0x68||addr==0x69);f->reads++;f->clock+=2;
 if(f->error)return f->error;
 if(f->short_read)return (int)len-1;
 memcpy(out,f->registers+reg,len);if(f->bad_reg==reg)out[0]^=1;return (int)len;
}
static int write_fake(void *ctx,uint8_t addr,uint8_t reg,const uint8_t *in,size_t len) {
 struct fake *f=ctx;assert(addr==0x68||addr==0x69);f->writes++;
 if(f->write_error)return f->write_error;
 if(f->short_write)return (int)len-1;
 if(f->error)return f->error;
 memcpy(f->registers+reg,in,len);
 if(reg==0x6b && *in==0x80&&!f->reset_stuck)f->registers[reg]=0x40;
 return (int)len;
}
static void wait_fake(void *ctx,uint32_t ms) {struct fake*f=ctx;f->clock+=ms;f->waits++;}
static uint64_t now_fake(void*ctx) {return ((struct fake*)ctx)->clock;}
static struct mpu_io make_io(struct fake*f) {struct mpu_io io={f,read_fake,write_fake,wait_fake,now_fake};return io;}
int main(void) {
 struct fake f={0};struct mpu_device d;struct mpu_raw raw;struct mpu_sample_state s={0};
 f.bad_reg=-1;f.registers[0x75]=0x68;
 assert(mpu_init(&d,make_io(&f),0x68)==MPU_OK);assert(d.ready && d.config.odr_hz==20 && f.waits==2);
 assert(d.config.accel==0&&d.config.gyro==0&&d.config.dlpf==3&&d.config.divider==49);
 uint8_t bytes[14]={0x80,0,0x7f,0xff,0xff,0xff,0x80,0,0x12,0x34,0xff,0xfe,0,0};
 assert(mpu_decode14(bytes,14,&raw));assert(raw.accel[0]==-32768&&raw.accel[1]==32767&&raw.accel[2]==-1&&raw.temperature==-32768&&raw.gyro[0]==4660&&raw.gyro[1]==-2);
 memcpy(f.registers+0x3b,bytes,14);uint64_t before=f.clock;
 assert(mpu_sample(&d,&s)==MPU_OK&&s.last_valid_ms==before+2&&s.sample_seq==1);
 struct mpu_sample_state saved=s;f.short_read=1;raw=s.last;
 assert(mpu_read_raw(&d,&raw)==MPU_SHORT&&!memcmp(&raw,&s.last,sizeof(raw)));
 assert(mpu_sample(&d,&s)==MPU_SHORT && !s.valid && s.sample_seq==saved.sample_seq&&s.last_valid_ms==saved.last_valid_ms);
 f.short_read=0;f.error=-MPU_NACK;assert(mpu_sample(&d,&s)==MPU_NACK);f.error=-MPU_TIMEOUT;assert(mpu_sample(&d,&s)==MPU_TIMEOUT);f.error=0;
 assert(mpu_sample(&d,&s)==MPU_OK&&s.valid&&s.sample_seq==2);
 unsigned reads=f.reads;struct mpu_io invalid=make_io(&f);invalid.read=0;
 assert(mpu_init(&d,invalid,0x68)==MPU_UNAVAILABLE&&!d.ready);
 assert(mpu_read_raw(&d,&raw)==MPU_UNAVAILABLE&&f.reads==reads);
 assert(mpu_init(&d,make_io(&f),0x69)==MPU_OK);
 assert(mpu_init(&d,make_io(&f),0x70)==MPU_UNAVAILABLE&&!d.ready);
 f.registers[0x75]=0x69;assert(mpu_init(&d,make_io(&f),0x68)==MPU_WRONG_ID&&!d.ready);f.registers[0x75]=0x68;
 f.reset_stuck=1;assert(mpu_init(&d,make_io(&f),0x68)==MPU_BAD_CONFIG&&!d.ready);f.reset_stuck=0;
 f.bad_reg=0x1a;assert(mpu_init(&d,make_io(&f),0x68)==MPU_BAD_CONFIG&&!d.ready);f.bad_reg=-1;
 f.write_error=-MPU_NACK;assert(mpu_init(&d,make_io(&f),0x68)==MPU_NACK&&!d.ready);
 f.write_error=-MPU_TIMEOUT;assert(mpu_init(&d,make_io(&f),0x68)==MPU_TIMEOUT&&!d.ready);
 f.write_error=0;f.short_write=1;assert(mpu_init(&d,make_io(&f),0x68)==MPU_SHORT&&!d.ready);f.short_write=0;
 for(int reg=0;reg<10;reg++){uint8_t regs[]={0x6b,0x6c,0x1a,0x19,0x1b,0x1c,0x23,0x37,0x38,0x6a};f.bad_reg=regs[reg];assert(mpu_init(&d,make_io(&f),0x68)==MPU_BAD_CONFIG&&!d.ready);}f.bad_reg=-1;
 f.error=-MPU_NACK;assert(mpu_init(&d,make_io(&f),0x68)==MPU_NACK&&!d.ready);
 f.error=-MPU_TIMEOUT;assert(mpu_init(&d,make_io(&f),0x68)==MPU_TIMEOUT&&!d.ready);
 f.error=0;f.short_read=1;assert(mpu_init(&d,make_io(&f),0x68)==MPU_SHORT&&!d.ready);
 assert(!mpu_decode14(bytes,13,&raw));puts("MPU6050_DRIVER_FAKE_PASS");return 0;
}
