/**
 * ============================================================================
 *  SkyPilot Drone - Motor Control Implementation
 * ============================================================================
 *  Controls 4 brushless motors via ESCs using the ESP32 LEDC PWM peripheral.
 *
 *  ESC Signal Protocol:
 *    - Frequency: 50 Hz (20ms period)
 *    - Pulse Width: 1000µs (off) to 2000µs (full throttle)
 *    - Arming: Hold 1000µs for 3 seconds after power-on
 *
 *  Motor Mixing (X-Configuration):
 *    M1 (Front-Left,  CCW): throttle + pitch + roll - yaw
 *    M2 (Front-Right, CW):  throttle + pitch - roll + yaw
 *    M3 (Rear-Left,   CW):  throttle - pitch + roll + yaw
 *    M4 (Rear-Right,  CCW): throttle - pitch - roll - yaw
 * ============================================================================
 */

#include "motor_control.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "MOTOR";

/* ── Motor pin mapping table ──────────────────────────────────────────────── */
static const int motor_pins[4] = {
    MOTOR1_PIN,             /* Motor 1: Front-Left  */
    MOTOR2_PIN,             /* Motor 2: Front-Right */
    MOTOR3_PIN,             /* Motor 3: Rear-Left   */
    MOTOR4_PIN              /* Motor 4: Rear-Right  */
};

static const int motor_channels[4] = {
    MOTOR1_LEDC_CHANNEL,
    MOTOR2_LEDC_CHANNEL,
    MOTOR3_LEDC_CHANNEL,
    MOTOR4_LEDC_CHANNEL
};

/* ═════════════════════════════════════════════════════════════════════════════
 *  Helper: Convert microseconds pulse to LEDC duty value
 *
 *  At 50 Hz with 16-bit resolution:
 *    Period = 20,000 µs
 *    Max duty = 2^16 = 65536
 *    duty = (pulse_us / 20000) * 65536
 * ═════════════════════════════════════════════════════════════════════════════ */
static uint32_t pulse_us_to_duty(uint16_t pulse_us)
{
    /* Clamp to valid ESC range */
    if (pulse_us < ESC_PULSE_MIN_US) pulse_us = ESC_PULSE_MIN_US;
    if (pulse_us > ESC_PULSE_MAX_US) pulse_us = ESC_PULSE_MAX_US;

    /* Calculate duty: (pulse_us / period_us) * max_duty */
    uint32_t max_duty = (1U << ESC_PWM_RESOLUTION) - 1;
    uint32_t period_us = 1000000 / ESC_PWM_FREQ_HZ;  /* 20000 µs at 50Hz */
    return (uint32_t)((uint64_t)pulse_us * max_duty / period_us);
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Helper: Constrain integer value
 * ═════════════════════════════════════════════════════════════════════════════ */
static inline uint16_t constrain_u16(int32_t val, uint16_t min_val, uint16_t max_val)
{
    if (val < (int32_t)min_val) return min_val;
    if (val > (int32_t)max_val) return max_val;
    return (uint16_t)val;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

esp_err_t motors_init(void)
{
    ESP_LOGI(TAG, "Initializing motor PWM channels...");

    /* ── Configure LEDC timer ─────────────────────────────────────────────── */
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = ESC_PWM_RESOLUTION,
        .timer_num       = MOTOR_LEDC_TIMER,
        .freq_hz         = ESC_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── Configure LEDC channels for each motor ───────────────────────────── */
    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = motor_channels[i],
            .timer_sel  = MOTOR_LEDC_TIMER,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = motor_pins[i],
            .duty       = pulse_us_to_duty(ESC_PULSE_MIN_US),  /* Start at minimum */
            .hpoint     = 0,
        };

        ret = ledc_channel_config(&ch_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config failed: %s",
                     i, esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "  Motor %d -> GPIO %d (LEDC ch %d)",
                 i + 1, motor_pins[i], motor_channels[i]);
    }

    ESP_LOGI(TAG, "All motor channels initialized at %d Hz, %d-bit resolution",
             ESC_PWM_FREQ_HZ, ESC_PWM_RESOLUTION);

    return ESP_OK;
}

esp_err_t motors_arm(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ARMING ESCs... STAND CLEAR!    ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════╝");

    /* Send minimum throttle to all ESCs for the arming duration */
    motors_stop();
    vTaskDelay(pdMS_TO_TICKS(ESC_ARM_DELAY_MS));

    ESP_LOGI(TAG, "ESCs armed successfully.");
    return ESP_OK;
}

void motor_set_pulse(int motor_num, uint16_t pulse_us)
{
    if (motor_num < 1 || motor_num > 4) {
        ESP_LOGW(TAG, "Invalid motor number: %d (use 1-4)", motor_num);
        return;
    }

    int idx = motor_num - 1;
    uint32_t duty = pulse_us_to_duty(pulse_us);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_channels[idx], duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_channels[idx]);
}

void motors_set_all(uint16_t m1_us, uint16_t m2_us,
                    uint16_t m3_us, uint16_t m4_us)
{
    motor_set_pulse(1, m1_us);
    motor_set_pulse(2, m2_us);
    motor_set_pulse(3, m3_us);
    motor_set_pulse(4, m4_us);
}

void motors_stop(void)
{
    motors_set_all(ESC_PULSE_MIN_US, ESC_PULSE_MIN_US,
                   ESC_PULSE_MIN_US, ESC_PULSE_MIN_US);
}

void motors_mix_and_apply(uint16_t throttle, float roll_pid,
                          float pitch_pid, float yaw_pid)
{
    /*
     *  X-Configuration Motor Mixing:
     *
     *         FRONT
     *     M1(CCW)  M2(CW)
     *         \  /
     *          \/
     *          /\
     *         /  \
     *     M3(CW)  M4(CCW)
     *         REAR
     *
     *  Motor 1 (Front-Left,  CCW): + pitch, + roll, - yaw
     *  Motor 2 (Front-Right, CW):  + pitch, - roll, + yaw
     *  Motor 3 (Rear-Left,   CW):  - pitch, + roll, + yaw
     *  Motor 4 (Rear-Right,  CCW): - pitch, - roll, - yaw
     */

    int32_t m1 = (int32_t)throttle + (int32_t)pitch_pid + (int32_t)roll_pid  - (int32_t)yaw_pid;
    int32_t m2 = (int32_t)throttle + (int32_t)pitch_pid - (int32_t)roll_pid  + (int32_t)yaw_pid;
    int32_t m3 = (int32_t)throttle - (int32_t)pitch_pid + (int32_t)roll_pid  + (int32_t)yaw_pid;
    int32_t m4 = (int32_t)throttle - (int32_t)pitch_pid - (int32_t)roll_pid  - (int32_t)yaw_pid;

    /* Constrain to valid ESC pulse range */
    uint16_t m1_us = constrain_u16(m1, ESC_PULSE_MIN_US, ESC_PULSE_MAX_US);
    uint16_t m2_us = constrain_u16(m2, ESC_PULSE_MIN_US, ESC_PULSE_MAX_US);
    uint16_t m3_us = constrain_u16(m3, ESC_PULSE_MIN_US, ESC_PULSE_MAX_US);
    uint16_t m4_us = constrain_u16(m4, ESC_PULSE_MIN_US, ESC_PULSE_MAX_US);

    /* Apply to hardware */
    motors_set_all(m1_us, m2_us, m3_us, m4_us);
}
