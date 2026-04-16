/**
 * ============================================================================
 *  SkyPilot Drone - Motor Control Header
 * ============================================================================
 *  Manages 4 brushless motors via ESCs using PWM signals.
 *  Provides initialization, arming, individual and group motor control.
 * ============================================================================
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "config.h"
#include "esp_err.h"

/**
 * Initialize the LEDC PWM channels for all 4 motors.
 * @return ESP_OK on success.
 */
esp_err_t motors_init(void);

/**
 * Send the ESC arming sequence (hold minimum throttle for ESC_ARM_DELAY_MS).
 * Must be called before any motor can spin.
 * @return ESP_OK when arming is complete.
 */
esp_err_t motors_arm(void);

/**
 * Set an individual motor's pulse width in microseconds.
 * @param motor_num  Motor number (1–4).
 * @param pulse_us   Pulse width in microseconds (1000–2000).
 */
void motor_set_pulse(int motor_num, uint16_t pulse_us);

/**
 * Set all 4 motors at once.
 * @param m1_us  Motor 1 pulse width (microseconds).
 * @param m2_us  Motor 2 pulse width (microseconds).
 * @param m3_us  Motor 3 pulse width (microseconds).
 * @param m4_us  Motor 4 pulse width (microseconds).
 */
void motors_set_all(uint16_t m1_us, uint16_t m2_us,
                    uint16_t m3_us, uint16_t m4_us);

/**
 * Immediately stop all motors (set to minimum pulse).
 * Use in emergency / failsafe situations.
 */
void motors_stop(void);

/**
 * Apply throttle and PID corrections using X-configuration motor mixing.
 *
 * @param throttle   Base throttle (1000–2000).
 * @param roll_pid   Roll PID output (-400 to +400).
 * @param pitch_pid  Pitch PID output (-400 to +400).
 * @param yaw_pid    Yaw PID output (-400 to +400).
 */
void motors_mix_and_apply(uint16_t throttle, float roll_pid,
                          float pitch_pid, float yaw_pid);

#endif /* MOTOR_CONTROL_H */
