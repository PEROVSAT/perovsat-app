#define DT_DRV_COMPAT perovsat_mpu6050_mock

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mpu6050_mock, CONFIG_LOG_DEFAULT_LEVEL);

struct mock_imu_data {
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
};

static int mock_imu_sample_fetch(const struct device *dev,
				 enum sensor_channel chan)
{
	struct mock_imu_data *data = dev->data;

	ARG_UNUSED(chan);

	data->accel[0] = (struct sensor_value){0, 0};
	data->accel[1] = (struct sensor_value){0, 0};
	data->accel[2] = (struct sensor_value){9, 807000};
	data->gyro[0] = (struct sensor_value){0, 0};
	data->gyro[1] = (struct sensor_value){0, 0};
	data->gyro[2] = (struct sensor_value){0, 0};

	return 0;
}

static int mock_imu_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	struct mock_imu_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_ACCEL_XYZ:
		memcpy(val, data->accel, 3 * sizeof(struct sensor_value));
		break;
	case SENSOR_CHAN_ACCEL_X:
		*val = data->accel[0];
		break;
	case SENSOR_CHAN_ACCEL_Y:
		*val = data->accel[1];
		break;
	case SENSOR_CHAN_ACCEL_Z:
		*val = data->accel[2];
		break;
	case SENSOR_CHAN_GYRO_XYZ:
		memcpy(val, data->gyro, 3 * sizeof(struct sensor_value));
		break;
	case SENSOR_CHAN_GYRO_X:
		*val = data->gyro[0];
		break;
	case SENSOR_CHAN_GYRO_Y:
		*val = data->gyro[1];
		break;
	case SENSOR_CHAN_GYRO_Z:
		*val = data->gyro[2];
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, mock_imu_api) = {
	.sample_fetch = mock_imu_sample_fetch,
	.channel_get = mock_imu_channel_get,
};

static int mock_imu_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

#define MOCK_IMU_DEFINE(inst)                                           \
	static struct mock_imu_data mock_imu_data_##inst;                  	\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mock_imu_init, NULL,             \
				     &mock_imu_data_##inst, NULL,            			\
				     POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, 			\
				     &mock_imu_api);

DT_INST_FOREACH_STATUS_OKAY(MOCK_IMU_DEFINE)
