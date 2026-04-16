/**
 * ============================================================================
 *  SkyPilot Drone - Flight Controller Header
 * ============================================================================
 *  The main flight control loop that ties everything together:
 *  reads IMU → processes PID → mixes motors → applies output.
 * ============================================================================
 */

#ifndef FLIGHT_CONTROLLER_H
#define FLIGHT_CONTROLLER_H

#include "config.h"
#include "esp_err.h"

/**
 * Initialize the flight controller (PID controllers, state machine).
 * Must be called AFTER imu_init() and motors_init().
 * @return ESP_OK on success.
 */
esp_err_t flight_controller_init(void);

/**
 * Start the flight controller task (runs the real-time control loop).
 * @return ESP_OK on success.
 */
esp_err_t flight_controller_start(void);

/**
 * Get the current drone state.
 * @return Current drone_state_t value.
 */
drone_state_t flight_controller_get_state(void);

/**
 * Get the current drone state as a human-readable string.
 * @return Pointer to static string describing the state.
 */
const char* flight_controller_get_state_str(void);

#endif /* FLIGHT_CONTROLLER_H */
