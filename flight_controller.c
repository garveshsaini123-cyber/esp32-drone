/**
 * ============================================================================
 *  SkyPilot Drone - Battery Monitor Header
 * ============================================================================
 *  Reads battery voltage via ADC and provides low-voltage warnings.
 * ============================================================================
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "config.h"
#include "esp_err.h"

/**
 * Initialize the ADC for battery voltage measurement.
 * @return ESP_OK on success.
 */
esp_err_t battery_monitor_init(void);

/**
 * Start the battery monitoring task.
 * @return ESP_OK on success.
 */
esp_err_t battery_monitor_start(void);

/**
 * Get the current battery voltage.
 * @return Voltage in volts.
 */
float battery_get_voltage(void);

/**
 * Get battery percentage estimate.
 * @return Percentage 0–100.
 */
uint8_t battery_get_percentage(void);

/**
 * Check if battery is critically low.
 * @return true if voltage below BATTERY_LOW_VOLTAGE.
 */
bool battery_is_critical(void);

/**
 * Check if battery is in warning range.
 * @return true if voltage below BATTERY_WARN_VOLTAGE.
 */
bool battery_is_low(void);

#endif /* BATTERY_MONITOR_H */
