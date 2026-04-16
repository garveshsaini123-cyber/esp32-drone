/**
 * ============================================================================
 *  SkyPilot Drone - Flight Controller Implementation
 * ============================================================================
 *  The main real-time control loop running at 250 Hz:
 *
 *    1. Read IMU sensor data (MPU6050)
 *    2. Get control input from phone (WiFi/UDP)
 *    3. Compute PID corrections for roll, pitch, yaw
 *    4. Mix motor outputs (X-configuration)
 *    5. Apply PWM signals to ESCs
 *    6. Handle state machine (init → calibrate → idle → armed → flying)
 *    7. Implement failsafe (auto-disarm on signal loss)
 *
 *  This task runs on Core 1 (the "app" core) at highest priority
 *  to ensure consistent loop timing.
 * ============================================================================
 */

#include "flight_controller.h"
#include "config.h"
#include "imu_sensor.h"
#include "pid_controller.h"
#include "motor_control.h"
#include "wifi_controller.h"
#include "battery_monitor.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "FLIGHT";

/* ── Current drone state ──────────────────────────────────────────────────── */
static volatile drone_state_t s_drone_state = STATE_INIT;

/* ── PID controllers for each axis ────────────────────────────────────────── */
static pid_t pid_roll;
static pid_t pid_pitch;
static pid_t pid_yaw;

/* ── State name lookup ────────────────────────────────────────────────────── */
static const char* state_names[] = {
    "INIT", "CALIBRATING", "IDLE", "ARMED",
    "FLYING", "LANDING", "FAILSAFE", "ERROR"
};

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void flight_control_task(void *pvParameters);

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

esp_err_t flight_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing flight controller...");

    /* ── Initialize PID controllers with default gains ────────────────────── */
    pid_init(&pid_roll,
             PID_ROLL_KP, PID_ROLL_KI, PID_ROLL_KD,
             PID_OUTPUT_MIN, PID_OUTPUT_MAX,
             PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);

    pid_init(&pid_pitch,
             PID_PITCH_KP, PID_PITCH_KI, PID_PITCH_KD,
             PID_OUTPUT_MIN, PID_OUTPUT_MAX,
             PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);

    pid_init(&pid_yaw,
             PID_YAW_KP, PID_YAW_KI, PID_YAW_KD,
             PID_OUTPUT_MIN, PID_OUTPUT_MAX,
             PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);

    ESP_LOGI(TAG, "PID controllers initialized:");
    ESP_LOGI(TAG, "  Roll:  Kp=%.2f Ki=%.3f Kd=%.2f",
             PID_ROLL_KP, PID_ROLL_KI, PID_ROLL_KD);
    ESP_LOGI(TAG, "  Pitch: Kp=%.2f Ki=%.3f Kd=%.2f",
             PID_PITCH_KP, PID_PITCH_KI, PID_PITCH_KD);
    ESP_LOGI(TAG, "  Yaw:   Kp=%.2f Ki=%.3f Kd=%.2f",
             PID_YAW_KP, PID_YAW_KI, PID_YAW_KD);

    s_drone_state = STATE_INIT;

    return ESP_OK;
}

esp_err_t flight_controller_start(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════╗");
    ESP_LOGI(TAG, "║  Starting Flight Control Loop    ║");
    ESP_LOGI(TAG, "║  Target: %3d Hz (%d µs period)   ║",
             FLIGHT_LOOP_FREQ_HZ, FLIGHT_LOOP_PERIOD_US);
    ESP_LOGI(TAG, "╚══════════════════════════════════╝");

    xTaskCreatePinnedToCore(
        flight_control_task,
        "flight_ctrl",
        FLIGHT_TASK_STACK,
        NULL,
        FLIGHT_TASK_PRIORITY,
        NULL,
        1   /* Run on Core 1 (app core) for real-time performance */
    );

    return ESP_OK;
}

drone_state_t flight_controller_get_state(void)
{
    return s_drone_state;
}

const char* flight_controller_get_state_str(void)
{
    int idx = (int)s_drone_state;
    if (idx >= 0 && idx < (int)(sizeof(state_names) / sizeof(state_names[0]))) {
        return state_names[idx];
    }
    return "UNKNOWN";
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Main Flight Control Task (Real-Time, 250 Hz)
 * ═════════════════════════════════════════════════════════════════════════════ */
static void flight_control_task(void *pvParameters)
{
    imu_data_t      imu_data;
    control_input_t ctrl_input;

    int64_t prev_time_us = esp_timer_get_time();
    int64_t loop_start_us;
    float   dt;

    uint32_t loop_count = 0;
    uint32_t failsafe_counter = 0;

    /* ── Initial calibration ──────────────────────────────────────────────── */
    s_drone_state = STATE_CALIBRATING;
    ESP_LOGI(TAG, ">>> Auto-calibrating IMU on startup...");
    esp_err_t cal_ret = imu_calibrate();
    if (cal_ret != ESP_OK) {
        ESP_LOGE(TAG, "IMU calibration FAILED! Check sensor wiring.");
        s_drone_state = STATE_ERROR;
        /* Continue to loop but won't allow arming */
    } else {
        s_drone_state = STATE_IDLE;
        ESP_LOGI(TAG, ">>> Calibration complete! State → IDLE");
    }

    /* ══════════════════════════════════════════════════════════════════════ */
    /*  MAIN CONTROL LOOP                                                    */
    /* ══════════════════════════════════════════════════════════════════════ */
    while (1) {
        loop_start_us = esp_timer_get_time();
        dt = (float)(loop_start_us - prev_time_us) / 1000000.0f;
        prev_time_us = loop_start_us;

        /* Sanity check dt (should be ~4ms at 250Hz) */
        if (dt <= 0.0f || dt > 0.1f) {
            dt = (float)FLIGHT_LOOP_PERIOD_US / 1000000.0f;
        }

        /* ── Step 1: Read IMU ─────────────────────────────────────────────── */
        esp_err_t imu_ret = imu_read(&imu_data, dt);
        if (imu_ret != ESP_OK) {
            ESP_LOGW(TAG, "IMU read failed!");
            /* Keep previous values, don't crash */
        }

        /* ── Step 2: Get control input from phone ─────────────────────────── */
        esp_err_t ctrl_ret = wifi_get_control_input(&ctrl_input);

        /* ── Step 3: Handle calibration request ───────────────────────────── */
        if (ctrl_input.calibrate && s_drone_state == STATE_IDLE) {
            ESP_LOGI(TAG, ">>> Recalibration requested!");
            s_drone_state = STATE_CALIBRATING;
            motors_stop();
            imu_calibrate();
            pid_reset(&pid_roll);
            pid_reset(&pid_pitch);
            pid_reset(&pid_yaw);
            s_drone_state = STATE_IDLE;
            ESP_LOGI(TAG, ">>> Recalibration complete!");
            continue;
        }

        /* ── Step 4: State Machine ────────────────────────────────────────── */
        switch (s_drone_state) {

        /* ─────────────────────────────────────────── IDLE ──────────────────*/
        case STATE_IDLE:
            motors_stop();

            /* Allow arming only if throttle is low */
            if (ctrl_input.armed &&
                ctrl_input.throttle <= THROTTLE_ARM_MAX &&
                ctrl_ret == ESP_OK)
            {
                ESP_LOGI(TAG, "╔══════════════════════════════════╗");
                ESP_LOGI(TAG, "║   ⚡ MOTORS ARMED! CAREFUL! ⚡   ║");
                ESP_LOGI(TAG, "╚══════════════════════════════════╝");

                motors_arm();
                pid_reset(&pid_roll);
                pid_reset(&pid_pitch);
                pid_reset(&pid_yaw);
                failsafe_counter = 0;
                s_drone_state = STATE_ARMED;
            }
            break;

        /* ─────────────────────────────────────────── ARMED / FLYING ────────*/
        case STATE_ARMED:
        case STATE_FLYING:
        {
            /* ── Failsafe: signal loss check ──────────────────────────────── */
            if (ctrl_ret == ESP_ERR_TIMEOUT) {
                failsafe_counter++;
                if (failsafe_counter > (FLIGHT_LOOP_FREQ_HZ * 2)) {  /* 2 sec */
                    ESP_LOGE(TAG, "!!! FAILSAFE TRIGGERED - Signal lost for 2s !!!");
                    motors_stop();
                    pid_reset(&pid_roll);
                    pid_reset(&pid_pitch);
                    pid_reset(&pid_yaw);
                    s_drone_state = STATE_FAILSAFE;
                    break;
                }
            } else {
                failsafe_counter = 0;
            }

            /* ── Disarm check ─────────────────────────────────────────────── */
            if (!ctrl_input.armed) {
                ESP_LOGI(TAG, ">>> DISARMED by pilot.");
                motors_stop();
                s_drone_state = STATE_IDLE;
                break;
            }

            /* ── Battery critical check ───────────────────────────────────── */
            if (battery_is_critical()) {
                ESP_LOGE(TAG, "!!! BATTERY CRITICAL - Auto-landing !!!");
                s_drone_state = STATE_LANDING;
                break;
            }

            /* ── Land request ─────────────────────────────────────────────── */
            if (ctrl_input.land_request) {
                ESP_LOGI(TAG, ">>> Landing requested.");
                s_drone_state = STATE_LANDING;
                break;
            }

            /* ── Update state based on throttle ───────────────────────────── */
            if (ctrl_input.throttle > THROTTLE_MIN + THROTTLE_DEADBAND) {
                s_drone_state = STATE_FLYING;
            }

            /* ── Compute target angles from stick input ───────────────────── */
            float target_roll  = ((float)ctrl_input.roll  / 128.0f) * MAX_ROLL_ANGLE;
            float target_pitch = ((float)ctrl_input.pitch / 128.0f) * MAX_PITCH_ANGLE;
            float target_yaw_rate = ((float)ctrl_input.yaw / 128.0f) * MAX_YAW_RATE;

            /* ── Set PID setpoints ────────────────────────────────────────── */
            pid_set_setpoint(&pid_roll,  target_roll);
            pid_set_setpoint(&pid_pitch, target_pitch);
            pid_set_setpoint(&pid_yaw,   target_yaw_rate);

            /* ── Compute PID outputs ──────────────────────────────────────── */
            float roll_correction  = pid_compute(&pid_roll,  imu_data.roll, dt);
            float pitch_correction = pid_compute(&pid_pitch, imu_data.pitch, dt);
            /* Yaw uses gyro rate directly (not angle) */
            float yaw_correction   = pid_compute(&pid_yaw,   imu_data.gyro_z, dt);

            /* ── Apply motor mixing ───────────────────────────────────────── */
            uint16_t throttle = ctrl_input.throttle;
            if (throttle < THROTTLE_MIN + THROTTLE_DEADBAND) {
                /* Throttle too low — don't spin motors but stay armed */
                motors_stop();
            } else {
                motors_mix_and_apply(throttle,
                                     roll_correction,
                                     pitch_correction,
                                     yaw_correction);
            }

            break;
        }

        /* ─────────────────────────────────────────── LANDING ───────────────*/
        case STATE_LANDING:
        {
            /* Simple auto-land: gradually reduce throttle */
            static uint16_t land_throttle = 0;
            if (land_throttle == 0) {
                land_throttle = ctrl_input.throttle > THROTTLE_MIN ?
                                ctrl_input.throttle : 1200;
            }

            /* Reduce throttle slowly (~50µs per second at 250Hz loop) */
            if (land_throttle > THROTTLE_MIN + 20) {
                land_throttle -= 1;  /* Very gradual descent */
            } else {
                /* Landed */
                motors_stop();
                land_throttle = 0;
                s_drone_state = STATE_IDLE;
                ESP_LOGI(TAG, ">>> Landed successfully. State → IDLE");
                break;
            }

            /* Still stabilize during landing */
            pid_set_setpoint(&pid_roll,  0.0f);
            pid_set_setpoint(&pid_pitch, 0.0f);
            pid_set_setpoint(&pid_yaw,   0.0f);

            float roll_c  = pid_compute(&pid_roll,  imu_data.roll, dt);
            float pitch_c = pid_compute(&pid_pitch, imu_data.pitch, dt);
            float yaw_c   = pid_compute(&pid_yaw,   imu_data.gyro_z, dt);

            motors_mix_and_apply(land_throttle, roll_c, pitch_c, yaw_c);
            break;
        }

        /* ─────────────────────────────────────────── FAILSAFE ──────────────*/
        case STATE_FAILSAFE:
            motors_stop();
            /* Stay in failsafe until phone reconnects and sends disarm */
            if (ctrl_ret == ESP_OK && !ctrl_input.armed) {
                ESP_LOGI(TAG, ">>> Signal restored. Disarmed. State → IDLE");
                s_drone_state = STATE_IDLE;
            }
            break;

        /* ─────────────────────────────────────────── ERROR ─────────────────*/
        case STATE_ERROR:
            motors_stop();
            /* Flash LED rapidly to indicate error */
            break;

        default:
            motors_stop();
            break;
        }

        /* ── Periodic telemetry logging (every 500 loops = 2 seconds) ──── */
        loop_count++;
        if (loop_count % 500 == 0) {
            ESP_LOGI(TAG, "[%s] R:%.1f° P:%.1f° Y:%.1f° | T:%d A:%d | dt:%.1fms",
                     flight_controller_get_state_str(),
                     imu_data.roll, imu_data.pitch, imu_data.yaw,
                     ctrl_input.throttle, ctrl_input.armed,
                     dt * 1000.0f);
        }

        /* ── Maintain precise loop timing ──────────────────────────────────── */
        int64_t elapsed_us = esp_timer_get_time() - loop_start_us;
        int64_t sleep_us = FLIGHT_LOOP_PERIOD_US - elapsed_us;
        if (sleep_us > 0) {
            vTaskDelay(pdMS_TO_TICKS(sleep_us / 1000));
        }
    }

    vTaskDelete(NULL);
}
