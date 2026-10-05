/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H
#include "cockpit/protocol/sensor_v1.h"
#include "mpu6050.h"
#define SENSOR_RUN_WINDOW_MS 900000u
struct sensor_service_io {
 void *context;
 int (*send)(void*,uint32_t,const uint8_t*,size_t); /* nonblocking, one try */
 int (*initialize)(void*,uint64_t); /* called only from independent worker */
};
struct sensor_service {
 struct sensor_service_io io;
 uint64_t epoch,session,core_epoch,subscription,lease_end,start_ms,next_publish,next_status;
 uint64_t sample_seq,publish_seq,recent[8];
 unsigned recent_next;
 uint32_t peer,errors,overwrites,send_failures,protocol_errors,control_drops;
 uint8_t initialized,ready_failed,pending,stopped,valid,status_dirty;
 uint16_t sample_error;
 struct mpu_raw raw;
 uint64_t sample_time;
};
void sensor_service_init(struct sensor_service*,struct sensor_service_io,uint64_t);
int sensor_service_request(struct sensor_service*,uint32_t,const uint8_t*,size_t,uint64_t);
void sensor_service_offer(struct sensor_service*,const struct mpu_sample_state*);
void sensor_service_poll(struct sensor_service*,uint64_t);
#endif
