/**
 * ============================================================================
 *  SkyPilot Drone - PID Controller Implementation
 * ============================================================================
 *  Generic PID controller with:
 *    - Anti-windup (integral clamping)
 *    - Derivative kick prevention
 *    - Output saturation
 * ============================================================================
 */

#include "pid_controller.h"

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

void pid_init(pid_t *pid, float kp, float ki, float kd,
              float output_min, float output_max,
              float integral_min, float integral_max)
{
    pid->kp           = kp;
    pid->ki           = ki;
    pid->kd           = kd;
    pid->setpoint     = 0.0f;
    pid->integral     = 0.0f;
    pid->prev_error   = 0.0f;
    pid->output       = 0.0f;
    pid->output_min   = output_min;
    pid->output_max   = output_max;
    pid->integral_min = integral_min;
    pid->integral_max = integral_max;
}

float pid_compute(pid_t *pid, float measurement, float dt)
{
    /* Guard against zero/negative dt */
    if (dt <= 0.0001f) {
        return pid->output;
    }

    /* ── Calculate error ──────────────────────────────────────────────────── */
    float error = pid->setpoint - measurement;

    /* ── Proportional term ────────────────────────────────────────────────── */
    float p_term = pid->kp * error;

    /* ── Integral term (with anti-windup clamping) ────────────────────────── */
    pid->integral += error * dt;

    /* Clamp the integral to prevent windup */
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }

    float i_term = pid->ki * pid->integral;

    /* ── Derivative term (on error, with dt normalization) ────────────────── */
    float d_term = pid->kd * (error - pid->prev_error) / dt;

    /* Store error for next iteration */
    pid->prev_error = error;

    /* ── Sum the terms ────────────────────────────────────────────────────── */
    float output = p_term + i_term + d_term;

    /* ── Clamp the output ─────────────────────────────────────────────────── */
    if (output > pid->output_max) {
        output = pid->output_max;
    } else if (output < pid->output_min) {
        output = pid->output_min;
    }

    pid->output = output;
    return output;
}

void pid_reset(pid_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
    pid->output     = 0.0f;
}

void pid_set_gains(pid_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void pid_set_setpoint(pid_t *pid, float setpoint)
{
    pid->setpoint = setpoint;
}
