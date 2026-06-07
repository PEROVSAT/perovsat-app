if(CONFIG_PEROVSAT_IMU_MOCK)
	target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mpu6050_mock.c)
endif()
