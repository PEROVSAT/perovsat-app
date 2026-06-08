#include "threads.hpp"

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

K_THREAD_DEFINE(payload_thread_id, ThreadConfig::DefaultStackSize, payload_entry,
                NULL, NULL, NULL, ThreadConfig::PayloadPriority, 0, -1);

void payload_entry(void *p1, void *p2, void *p3) {
    const struct device *imu = DEVICE_DT_GET(DT_ALIAS(imu));

    LOG_INF("Payload Thread Started");

    if (!device_is_ready(imu)) {
        LOG_ERR("IMU not ready");
        return;
    }

    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    while (1) {
        sensor_sample_fetch(imu);
        sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
        sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);

        LOG_INF("Accel: %d.%06d %d.%06d %d.%06d m/s^2",
                accel[0].val1, accel[0].val2,
                accel[1].val1, accel[1].val2,
                accel[2].val1, accel[2].val2);
        LOG_INF("Gyro: %d.%06d %d.%06d %d.%06d rad/s",
                gyro[0].val1, gyro[0].val2,
                gyro[1].val1, gyro[1].val2,
                gyro[2].val1, gyro[2].val2);

        k_sleep(K_MSEC(2000));
    }
}
