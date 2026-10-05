/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H
#include <rtthread.h>
#include "mpu6050.h"
rt_err_t mpu_sensor_resource_ready(uint64_t);
enum mpu_error mpu_sensor_last_error(void);
void mpu_sensor_report(const struct mpu_sample_state*);
void mpu_sensor_request_stop(void);
int mpu_sensor_has_stopped(void);
#endif
