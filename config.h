/**
 * ============================================================================
 *  SkyPilot Drone - Battery Monitor Implementation
 * ============================================================================
 *  Reads battery voltage through a resistor divider connected to an ADC pin.
 *  Provides voltage, percentage, and low-battery warnings.
 *
 *  Wiring:
 *    Battery (+) ──[100kΩ]──┬──[10kΩ]── GND
 *                           │
 *                       GPIO 34 (ADC1_CH6)
 *
 *  Voltage divider ratio: (100k + 10k) / 10k = 11.0
 *  So: V_battery = V_adc × 11.0
 * ============================================================================
 */

#include "battery_monitor.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "BATTERY";

/* ── State ────────────────────────────────────────────────────────────────── */
static float    s_battery_voltage  = 0.0f;
static uint8_t  s_battery_percent  = 0;
static bool     s_is_critical      = false;
static bool     s_is_low           = false;

static esp_adc_cal_characteristics_t s_adc_chars;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void battery_monitor_task(void *pvParameters);
static float read_battery_voltage(void);

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

esp_err_t battery_monitor_init(void)
{
    ESP_LOGI(TAG, "Initializing battery monitor on GPIO %d...", BATTERY_ADC_PIN);

    /* Configure ADC1 Channel 6 (GPIO 34) */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    /* Characterize ADC for better accuracy */
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12, 1100, &s_adc_chars);

    /* Take initial reading */
    s_battery_voltage = read_battery_voltage();
    ESP_LOGI(TAG, "Initial battery voltage: %.2f V", s_battery_voltage);

    return ESP_OK;
}

esp_err_t battery_monitor_start(void)
{
    xTaskCreatePinnedToCore(
        battery_monitor_task,
        "battery_mon",
        BATTERY_TASK_STACK,
        NULL,
        BATTERY_TASK_PRIORITY,
        NULL,
        0   /* Run on core 0 */
    );

    ESP_LOGI(TAG, "Battery monitor task started.");
    return ESP_OK;
}

float battery_get_voltage(void)
{
    return s_battery_voltage;
}

uint8_t battery_get_percentage(void)
{
    return s_battery_percent;
}

bool battery_is_critical(void)
{
    return s_is_critical;
}

bool battery_is_low(void)
{
    return s_is_low;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Internal: Read and average ADC samples
 * ═════════════════════════════════════════════════════════════════════════════ */
static float read_battery_voltage(void)
{
    /* Average 32 samples for noise reduction */
    uint32_t adc_sum = 0;
    const int num_samples = 32;

    for (int i = 0; i < num_samples; i++) {
        uint32_t mv = 0;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_6, &s_adc_chars, &mv);
        adc_sum += mv;
    }

    float adc_voltage = (float)(adc_sum / num_samples) / 1000.0f;  /* mV → V */
    float battery_v = adc_voltage * BATTERY_VOLTAGE_DIVIDER;

    return battery_v;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Internal: Convert voltage to percentage (3S LiPo curve approximation)
 * ═════════════════════════════════════════════════════════════════════════════ */
static uint8_t voltage_to_percent(float voltage)
{
    /* 3S LiPo voltage range: 9.0V (empty) to 12.6V (full) */
    float cell_voltage = voltage / 3.0f;

    /* Simplified LiPo discharge curve */
    if (cell_voltage >= 4.20f) return 100;
    if (cell_voltage >= 4.15f) return 95;
    if (cell_voltage >= 4.10f) return 90;
    if (cell_voltage >= 4.05f) return 85;
    if (cell_voltage >= 4.00f) return 80;
    if (cell_voltage >= 3.95f) return 75;
    if (cell_voltage >= 3.90f) return 70;
    if (cell_voltage >= 3.85f) return 65;
    if (cell_voltage >= 3.80f) return 60;
    if (cell_voltage >= 3.75f) return 50;
    if (cell_voltage >= 3.70f) return 40;
    if (cell_voltage >= 3.65f) return 30;
    if (cell_voltage >= 3.60f) return 20;
    if (cell_voltage >= 3.55f) return 15;
    if (cell_voltage >= 3.50f) return 10;
    if (cell_voltage >= 3.40f) return 5;
    return 0;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Battery Monitor Task (runs every 2 seconds)
 * ═════════════════════════════════════════════════════════════════════════════ */
static void battery_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Battery monitor running (2s interval).");

    while (1) {
        s_battery_voltage = read_battery_voltage();
        s_battery_percent = voltage_to_percent(s_battery_voltage);

        /* Update warning flags */
        s_is_critical = (s_battery_voltage < BATTERY_LOW_VOLTAGE) &&
                        (s_battery_voltage > 1.0f);  /* Ignore if no battery */
        s_is_low = (s_battery_voltage < BATTERY_WARN_VOLTAGE) &&
                   (s_battery_voltage > 1.0f);

        /* Log warnings */
        if (s_is_critical) {
            ESP_LOGE(TAG, "!!! BATTERY CRITICAL: %.2fV (%d%%) - LAND NOW !!!",
                     s_battery_voltage, s_battery_percent);
        } else if (s_is_low) {
            ESP_LOGW(TAG, "⚠ Battery low: %.2fV (%d%%)",
                     s_battery_voltage, s_battery_percent);
        }

        /* Normal periodic log (every 10 seconds = every 5 iterations) */
        static int log_counter = 0;
        log_counter++;
        if (log_counter % 5 == 0) {
            ESP_LOGI(TAG, "Battery: %.2fV (%d%%)", s_battery_voltage, s_battery_percent);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(NULL);
}
