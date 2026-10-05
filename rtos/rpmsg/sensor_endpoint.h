/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SENSOR_ENDPOINT_H
#define SENSOR_ENDPOINT_H
#include <rtthread.h>
#include "rpmsg_lite.h"
rt_err_t sensor_endpoint_start(struct rpmsg_lite_instance*,rt_tick_t);
rt_bool_t sensor_endpoint_stop(void);
#endif
