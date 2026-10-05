/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MPU6050_H
#define MPU6050_H
#include <stddef.h>
#include <stdint.h>
enum mpu_error { MPU_OK=0, MPU_NACK=1, MPU_TIMEOUT=2, MPU_SHORT=3, MPU_WRONG_ID=4, MPU_BAD_CONFIG=5, MPU_UNAVAILABLE=6 };
/* read/write return bytes transferred or negative mpu_error; no hidden retry. */
struct mpu_io { void *context; int (*read)(void*,uint8_t,uint8_t,uint8_t*,size_t); int (*write)(void*,uint8_t,uint8_t,const uint8_t*,size_t); void (*wait_ms)(void*,uint32_t); uint64_t (*now_ms)(void*); };
struct mpu_config { uint8_t who, power, accel, gyro, dlpf, divider; uint32_t config_id, odr_hz; };
struct mpu_raw { int16_t accel[3], temperature, gyro[3]; };
struct mpu_device { struct mpu_io io; uint8_t address, ready; struct mpu_config config; };
struct mpu_sample_state { struct mpu_raw last; uint64_t sample_seq, last_valid_ms; uint32_t attempts, errors; enum mpu_error error; uint8_t valid; };
enum mpu_error mpu_init(struct mpu_device*,struct mpu_io,uint8_t);
enum mpu_error mpu_read_raw(struct mpu_device*,struct mpu_raw*);
enum mpu_error mpu_sample(struct mpu_device*,struct mpu_sample_state*);
int mpu_decode14(const uint8_t*,size_t,struct mpu_raw*);
#endif
