/* SPDX-License-Identifier: Apache-2.0 */
#include "mpu6050.h"
#include <string.h>
static enum mpu_error result(int n,size_t expected) {
    if (n==(int)expected)
    return MPU_OK;
    if (n>=0)
    return MPU_SHORT;
    if (n==-MPU_NACK)
    return MPU_NACK;
    if (n==-MPU_TIMEOUT)
    return MPU_TIMEOUT;
    return MPU_UNAVAILABLE;
}
static enum mpu_error rd(struct mpu_device*d,uint8_t r,uint8_t*p,size_t n) {
    return result(d->io.read(d->io.context,d->address,r,p,n),n);
}
static enum mpu_error wr(struct mpu_device*d,uint8_t r,uint8_t v) {
    return result(d->io.write(d->io.context,d->address,r,&v,1),1);
}
enum mpu_error mpu_init(struct mpu_device*d,struct mpu_io io,uint8_t address) {
    static const uint8_t regs[]={0x6b,0x6c,0x1a,0x19,0x1b,0x1c,0x23,0x37,0x38,0x6a};
    static const uint8_t values[]={1,0,3,49,0,0,0,0,0,0};
    uint8_t v;
    size_t i;
    enum mpu_error e;
    if (!d)
    return MPU_UNAVAILABLE;
    memset(d, 0, sizeof(*d));
    if (!io.read || !io.write || !io.wait_ms || !io.now_ms ||
    (address != 0x68 && address != 0x69))
    return MPU_UNAVAILABLE;
    d->io=io;
    d->address=address;
    if ((e=rd(d,0x75,&v,1))!=MPU_OK)
    return e;
    if (v != 0x68)
    return MPU_WRONG_ID;
    d->config.who=v;
    /* DEVICE_RESET then a bounded settling delay and reset-bit readback. */
    if ((e=wr(d,0x6b,0x80))!=MPU_OK)
    return e;
    io.wait_ms(io.context,100);
    if ((e=rd(d,0x6b,&v,1))!=MPU_OK)
    return e;
    if (v&0x80)
    return MPU_BAD_CONFIG;
    for (i = 0; i < sizeof(regs); i++) {
        if ((e=wr(d,regs[i],values[i]))!=MPU_OK)
        return e;
    }
    io.wait_ms(io.context,100);
    for (i = 0; i < sizeof(regs); i++) {
        if ((e=rd(d,regs[i],&v,1))!=MPU_OK)
        return e;
        if (v!=values[i])
        return MPU_BAD_CONFIG;
    }
    if ((e=rd(d,0x75,&v,1))!=MPU_OK)
    return e;
    if (v != 0x68)
    return MPU_WRONG_ID;
    d->config.power=1;
    d->config.accel=0;
    d->config.gyro=0;
    d->config.dlpf=3;
    d->config.divider=49;
    d->config.odr_hz=20;
    d->config.config_id=0x00010331;
    d->ready=1;
    return MPU_OK;
}
static int16_t signed_be(const uint8_t*p) { uint32_t u=((uint32_t)p[0]<<8)|p[1];
    int32_t v=u>=32768 ? (int32_t)u-65536:(int32_t)u;
    return (int16_t)v;
}
int mpu_decode14(const uint8_t*p,size_t n,struct mpu_raw*r) {
    unsigned i;
    if (!p||!r||n!=14)
    return 0;
    for (i = 0; i < 3; i++)r->accel[i]=signed_be(p+2*i);
    r->temperature=signed_be(p+6);
    for (i = 0; i < 3; i++)r->gyro[i]=signed_be(p+8+2*i);
    return 1;
}
enum mpu_error mpu_read_raw(struct mpu_device*d,struct mpu_raw*r) {
    uint8_t bytes[14];
    enum mpu_error e;
    if (!d||!r||!d->ready)
    return MPU_UNAVAILABLE;
    e=rd(d,0x3b,bytes,sizeof(bytes));
    if (e!=MPU_OK)
    return e;
    return mpu_decode14(bytes,sizeof(bytes),r)?MPU_OK:MPU_SHORT;
}
enum mpu_error mpu_sample(struct mpu_device*d,struct mpu_sample_state*s) {
    struct mpu_raw raw;
    enum mpu_error e;
    if (!s)
    return MPU_UNAVAILABLE;
    s->attempts++;
    e=mpu_read_raw(d,&raw);
    s->error=e;
    if (e != MPU_OK) {
        s->errors++;
        s->valid=0;
        return e;
    }
    s->last=raw;
    s->sample_seq++;
    s->last_valid_ms=d->io.now_ms(d->io.context);
    s->valid=1;
    return MPU_OK;
}
