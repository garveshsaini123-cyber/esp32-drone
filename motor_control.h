/**
 * ============================================================================
 *  SkyPilot Drone - IMU Sensor (MPU6050) Header
 * ============================================================================
 *  I2C driver for the MPU6050 6-axis accelerometer/gyroscope.
 *  Provides calibration, raw data reading, and complementary filter fusion.
 * ============================================================================
 */

#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include "config.h"
#include "esp_err.h"

/**
 * Initialize the I2C bus and configure the MPU6050 sensor.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t imu_init(void);

/**
 * Calibrate the IMU by collecting baseline offset samples.
 * Drone MUST be stationary and level during calibration.
 * @return ESP_OK on success.
 */
esp_err_t imu_calibrate(void);

/**
 * Read raw accelerometer and gyroscope data and apply
 * complementary filter to compute roll, pitch, yaw angles.
 * @param data Pointer to imu_data_t struct to fill.
 * @param dt   Delta time since last reading in seconds.
 * @return ESP_OK on success.
 */
esp_err_t imu_read(imu_data_t *data, float dt);

/**
 * Get the current temperature from the MPU6050 built-in sensor.
 * @return Temperature in degrees Celsius.
 */
float imu_get_temperature(void);

/**
 * Check if the MPU6050 is connected and responding.
 * @return true if WHO_AM_I register returns correct value.
 */
bool imu_is_connected(void);

#endif /* IMU_SENSOR_H */
