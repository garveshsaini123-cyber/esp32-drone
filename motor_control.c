/**
 * ============================================================================
 *  SkyPilot Drone - IMU Sensor (MPU6050) Implementation
 * ============================================================================
 *  Full I2C driver for the MPU6050 sensor including:
 *    - I2C bus initialization
 *    - MPU6050 configuration (sample rate, range, DLPF)
 *    - Gyroscope & accelerometer offset calibration
 *    - Raw data reading
 *    - Complementary filter for attitude estimation (roll, pitch, yaw)
 * ============================================================================
 */

#include "imu_sensor.h"
#include "config.h"

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "IMU";

/* ── Calibration offsets ──────────────────────────────────────────────────── */
static float gyro_offset_x  = 0.0f;
static float gyro_offset_y  = 0.0f;
static float gyro_offset_z  = 0.0f;
static float accel_offset_x = 0.0f;
static float accel_offset_y = 0.0f;
static float accel_offset_z = 0.0f;

/* ── Filtered angles (persistent state) ───────────────────────────────────── */
static float filtered_roll  = 0.0f;
static float filtered_pitch = 0.0f;
static float filtered_yaw   = 0.0f;

/* ── Calibration flag ─────────────────────────────────────────────────────── */
static bool is_calibrated = false;

/* ═════════════════════════════════════════════════════════════════════════════
 *  Low-Level I2C Helpers
 * ═════════════════════════════════════════════════════════════════════════════ */

/**
 * Write a single byte to a register on the MPU6050.
 */
static esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * Read a single byte from a register.
 */
static esp_err_t mpu6050_read_byte(uint8_t reg, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  /* Repeated start */
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * Read multiple bytes starting from a register (burst read).
 */
static esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *buffer, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, buffer, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, buffer + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

esp_err_t imu_init(void)
{
    esp_err_t ret;

    /* ── Configure I2C master ─────────────────────────────────────────────── */
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    ret = i2c_param_config(I2C_PORT_NUM, &i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d, %d Hz)",
             I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

    /* ── Verify MPU6050 is connected ──────────────────────────────────────── */
    if (!imu_is_connected()) {
        ESP_LOGE(TAG, "MPU6050 not found! Check wiring.");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "MPU6050 detected at address 0x%02X", MPU6050_ADDR);

    /* ── Wake up MPU6050 (clear sleep bit, use X-axis gyro as clock) ──────── */
    ret = mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0x01);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* ── Set sample rate divider: 1kHz / (1+3) = 250 Hz ──────────────────── */
    ret = mpu6050_write_byte(MPU6050_SMPLRT_DIV, 0x03);
    if (ret != ESP_OK) return ret;

    /* ── Configure Digital Low Pass Filter (DLPF): ~44Hz bandwidth ────────── */
    ret = mpu6050_write_byte(MPU6050_CONFIG, 0x03);
    if (ret != ESP_OK) return ret;

    /* ── Gyroscope config: ±250°/s (most sensitive for stability) ─────────── */
    ret = mpu6050_write_byte(MPU6050_GYRO_CONFIG, 0x00);
    if (ret != ESP_OK) return ret;

    /* ── Accelerometer config: ±2g (most sensitive) ───────────────────────── */
    ret = mpu6050_write_byte(MPU6050_ACCEL_CONFIG, 0x00);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "MPU6050 configured: ±250°/s gyro, ±2g accel, 250Hz sample rate");

    return ESP_OK;
}

bool imu_is_connected(void)
{
    uint8_t who_am_i = 0;
    esp_err_t ret = mpu6050_read_byte(MPU6050_WHO_AM_I_REG, &who_am_i);
    if (ret == ESP_OK && who_am_i == MPU6050_WHO_AM_I_VAL) {
        return true;
    }
    ESP_LOGW(TAG, "WHO_AM_I returned 0x%02X (expected 0x%02X)",
             who_am_i, MPU6050_WHO_AM_I_VAL);
    return false;
}

esp_err_t imu_calibrate(void)
{
    ESP_LOGI(TAG, "Starting IMU calibration (%d samples)...", IMU_CALIBRATION_SAMPLES);
    ESP_LOGW(TAG, ">>> KEEP THE DRONE FLAT AND STATIONARY! <<<");

    float gx_sum = 0, gy_sum = 0, gz_sum = 0;
    float ax_sum = 0, ay_sum = 0, az_sum = 0;

    uint8_t buffer[14];

    for (int i = 0; i < IMU_CALIBRATION_SAMPLES; i++) {
        esp_err_t ret = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H, buffer, 14);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Calibration read failed at sample %d", i);
            return ret;
        }

        /* Parse raw values (big-endian 16-bit signed) */
        int16_t raw_ax = (int16_t)((buffer[0]  << 8) | buffer[1]);
        int16_t raw_ay = (int16_t)((buffer[2]  << 8) | buffer[3]);
        int16_t raw_az = (int16_t)((buffer[4]  << 8) | buffer[5]);
        /* buffer[6..7] = temperature, skip */
        int16_t raw_gx = (int16_t)((buffer[8]  << 8) | buffer[9]);
        int16_t raw_gy = (int16_t)((buffer[10] << 8) | buffer[11]);
        int16_t raw_gz = (int16_t)((buffer[12] << 8) | buffer[13]);

        ax_sum += (float)raw_ax / ACCEL_SENSITIVITY;
        ay_sum += (float)raw_ay / ACCEL_SENSITIVITY;
        az_sum += (float)raw_az / ACCEL_SENSITIVITY;
        gx_sum += (float)raw_gx / GYRO_SENSITIVITY;
        gy_sum += (float)raw_gy / GYRO_SENSITIVITY;
        gz_sum += (float)raw_gz / GYRO_SENSITIVITY;

        vTaskDelay(pdMS_TO_TICKS(2));  /* ~500 Hz sampling for calibration */
    }

    float n = (float)IMU_CALIBRATION_SAMPLES;

    /* Gyro offsets: should be zero at rest */
    gyro_offset_x = gx_sum / n;
    gyro_offset_y = gy_sum / n;
    gyro_offset_z = gz_sum / n;

    /* Accel offsets: X and Y should be zero, Z should be 1.0g */
    accel_offset_x = ax_sum / n;
    accel_offset_y = ay_sum / n;
    accel_offset_z = (az_sum / n) - 1.0f;  /* Subtract gravity */

    ESP_LOGI(TAG, "Calibration complete!");
    ESP_LOGI(TAG, "  Gyro offsets:  X=%.4f  Y=%.4f  Z=%.4f (°/s)",
             gyro_offset_x, gyro_offset_y, gyro_offset_z);
    ESP_LOGI(TAG, "  Accel offsets: X=%.4f  Y=%.4f  Z=%.4f (g)",
             accel_offset_x, accel_offset_y, accel_offset_z);

    /* Reset filter state */
    filtered_roll  = 0.0f;
    filtered_pitch = 0.0f;
    filtered_yaw   = 0.0f;

    is_calibrated = true;
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *data, float dt)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;

    /* ── Burst-read all 14 bytes (accel + temp + gyro) ────────────────────── */
    uint8_t buffer[14];
    esp_err_t ret = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H, buffer, 14);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read IMU data");
        return ret;
    }

    /* ── Parse raw values ─────────────────────────────────────────────────── */
    int16_t raw_ax = (int16_t)((buffer[0]  << 8) | buffer[1]);
    int16_t raw_ay = (int16_t)((buffer[2]  << 8) | buffer[3]);
    int16_t raw_az = (int16_t)((buffer[4]  << 8) | buffer[5]);
    int16_t raw_temp = (int16_t)((buffer[6] << 8) | buffer[7]);
    int16_t raw_gx = (int16_t)((buffer[8]  << 8) | buffer[9]);
    int16_t raw_gy = (int16_t)((buffer[10] << 8) | buffer[11]);
    int16_t raw_gz = (int16_t)((buffer[12] << 8) | buffer[13]);

    /* ── Convert to physical units and subtract offsets ────────────────────── */
    data->accel_x = ((float)raw_ax / ACCEL_SENSITIVITY) - accel_offset_x;
    data->accel_y = ((float)raw_ay / ACCEL_SENSITIVITY) - accel_offset_y;
    data->accel_z = ((float)raw_az / ACCEL_SENSITIVITY) - accel_offset_z;

    data->gyro_x = ((float)raw_gx / GYRO_SENSITIVITY) - gyro_offset_x;
    data->gyro_y = ((float)raw_gy / GYRO_SENSITIVITY) - gyro_offset_y;
    data->gyro_z = ((float)raw_gz / GYRO_SENSITIVITY) - gyro_offset_z;

    /* Temperature formula from MPU6050 datasheet */
    data->temperature = ((float)raw_temp / 340.0f) + 36.53f;

    /* ── Compute angles from accelerometer (atan2) ────────────────────────── */
    float accel_roll  = atan2f(data->accel_y, data->accel_z) * (180.0f / M_PI);
    float accel_pitch = atan2f(-data->accel_x,
                               sqrtf(data->accel_y * data->accel_y +
                                     data->accel_z * data->accel_z))
                        * (180.0f / M_PI);

    /* ── Complementary filter ─────────────────────────────────────────────── */
    /*
     *  The complementary filter fuses:
     *    - Gyroscope (fast, accurate short-term, drifts long-term)
     *    - Accelerometer (noisy short-term, accurate long-term)
     *
     *  angle = α * (angle + gyro * dt) + (1 - α) * accel_angle
     *  where α = COMP_FILTER_ALPHA (typically 0.96–0.98)
     */
    filtered_roll  = COMP_FILTER_ALPHA * (filtered_roll  + data->gyro_x * dt)
                   + (1.0f - COMP_FILTER_ALPHA) * accel_roll;

    filtered_pitch = COMP_FILTER_ALPHA * (filtered_pitch + data->gyro_y * dt)
                   + (1.0f - COMP_FILTER_ALPHA) * accel_pitch;

    /* Yaw has no accelerometer reference (no magnetometer), so gyro-only */
    filtered_yaw  += data->gyro_z * dt;

    /* Keep yaw in [-180, 180] range */
    if (filtered_yaw > 180.0f)  filtered_yaw -= 360.0f;
    if (filtered_yaw < -180.0f) filtered_yaw += 360.0f;

    data->roll  = filtered_roll;
    data->pitch = filtered_pitch;
    data->yaw   = filtered_yaw;

    return ESP_OK;
}

float imu_get_temperature(void)
{
    uint8_t buffer[2];
    if (mpu6050_read_bytes(MPU6050_TEMP_OUT_H, buffer, 2) != ESP_OK) {
        return -999.0f;
    }
    int16_t raw = (int16_t)((buffer[0] << 8) | buffer[1]);
    return ((float)raw / 340.0f) + 36.53f;
}
