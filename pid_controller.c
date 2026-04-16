/**
 * ============================================================================
 *  SkyPilot Drone - Main Entry Point
 * ============================================================================
 *  Firmware entry point for the ESP32 Flight Controller.
 *
 *  Initialization sequence:
 *    1. Print boot banner
 *    2. I2C scanner (debug: find MPU6050 address)
 *    3. Initialize IMU sensor (MPU6050)
 *    4. Initialize motor PWM channels
 *    5. Initialize WiFi Access Point
 *    6. Start HTTP web server + WebSocket
 *    7. Initialize battery monitor
 *    8. Initialize and start flight controller
 *    9. Status LED blink loop
 *
 *  If any initialization step fails, the system enters ERROR state
 *  and blinks the LED rapidly to indicate the problem.
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "imu_sensor.h"
#include "motor_control.h"
#include "wifi_controller.h"
#include "flight_controller.h"
#include "battery_monitor.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "MAIN";

/* ═════════════════════════════════════════════════════════════════════════════
 *  I2C Address Scanner (Debug Utility)
 *
 *  Scans all 127 possible I2C addresses and reports which devices respond.
 *  This helps debug the "MPU testConnection: FAIL" issue you're seeing.
 *
 *  Common MPU6050 addresses:
 *    0x68 — AD0 pin is LOW (most modules)
 *    0x69 — AD0 pin is HIGH
 * ═════════════════════════════════════════════════════════════════════════════ */
static void i2c_scanner(void)
{
    ESP_LOGI(TAG, "┌─────────────────────────────────┐");
    ESP_LOGI(TAG, "│   I2C Bus Scanner               │");
    ESP_LOGI(TAG, "│   SDA=GPIO%d  SCL=GPIO%d        │", I2C_SDA_PIN, I2C_SCL_PIN);
    ESP_LOGI(TAG, "└─────────────────────────────────┘");

    int devices_found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  ✓ Device found at address 0x%02X (%d)", addr, addr);
            devices_found++;

            /* Identify known devices */
            if (addr == 0x68 || addr == 0x69) {
                ESP_LOGI(TAG, "    └── MPU6050 / MPU6500 / MPU9250");
            } else if (addr == 0x1E) {
                ESP_LOGI(TAG, "    └── HMC5883L Magnetometer");
            } else if (addr == 0x76 || addr == 0x77) {
                ESP_LOGI(TAG, "    └── BMP280 / BME280 Barometer");
            } else if (addr == 0x53) {
                ESP_LOGI(TAG, "    └── ADXL345 Accelerometer");
            }
        }
    }

    if (devices_found == 0) {
        ESP_LOGE(TAG, "  ✗ NO I2C devices found!");
        ESP_LOGE(TAG, "    Troubleshooting:");
        ESP_LOGE(TAG, "    1. Check SDA wire → GPIO %d", I2C_SDA_PIN);
        ESP_LOGE(TAG, "    2. Check SCL wire → GPIO %d", I2C_SCL_PIN);
        ESP_LOGE(TAG, "    3. Check VCC (3.3V) and GND connections");
        ESP_LOGE(TAG, "    4. Add 4.7kΩ pull-up resistors on SDA & SCL");
        ESP_LOGE(TAG, "    5. Try different MPU6050 module (may be dead)");
    } else {
        ESP_LOGI(TAG, "  Total: %d device(s) found on I2C bus.", devices_found);
    }
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Status LED blink patterns
 * ═════════════════════════════════════════════════════════════════════════════ */
static void status_led_task(void *pvParameters)
{
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        drone_state_t state = flight_controller_get_state();

        switch (state) {
        case STATE_INIT:
        case STATE_CALIBRATING:
            /* Fast blink: 100ms on/off */
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case STATE_IDLE:
            /* Slow breathe: 1s on, 1s off */
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            gpio_set_level(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case STATE_ARMED:
            /* Double blink */
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(700));
            break;

        case STATE_FLYING:
            /* Solid ON */
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case STATE_LANDING:
            /* Triple blink */
            for (int i = 0; i < 3; i++) {
                gpio_set_level(STATUS_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(80));
                gpio_set_level(STATUS_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(80));
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case STATE_FAILSAFE:
        case STATE_ERROR:
            /* Very fast blink (SOS) */
            gpio_set_level(STATUS_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(STATUS_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;

        default:
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
    }
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Application Entry Point
 * ═════════════════════════════════════════════════════════════════════════════ */
void app_main(void)
{
    /* ══════════════════════════════════════════════════════════════════════ */
    /*  BOOT BANNER                                                         */
    /* ══════════════════════════════════════════════════════════════════════ */
    printf("\n");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "║   ▲ SkyPilot Flight Controller v%d.%d.%d       ║",
             FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    ESP_LOGI(TAG, "║   ESP32 Quadcopter Firmware                  ║");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "║   WiFi: %s                    ║", WIFI_AP_SSID);
    ESP_LOGI(TAG, "║   Control: http://192.168.4.1                ║");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
    printf("\n");

    /* ── Initialize NVS (required for WiFi) ───────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "[1/7] NVS Flash initialized ✓");

    /* ── I2C Scanner (debug MPU6050 connection) ───────────────────────────── */
    ESP_LOGI(TAG, "[2/7] Scanning I2C bus...");

    /* Temporarily init I2C for scanning */
    i2c_config_t scan_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_PORT_NUM, &scan_cfg);
    i2c_driver_install(I2C_PORT_NUM, I2C_MODE_MASTER, 0, 0, 0);
    i2c_scanner();
    i2c_driver_delete(I2C_PORT_NUM);  /* Will be re-initialized by imu_init() */

    /* ── Initialize IMU (MPU6050) ─────────────────────────────────────────── */
    ESP_LOGI(TAG, "[3/7] Initializing IMU sensor...");
    ret = imu_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "╔══════════════════════════════════════════════╗");
        ESP_LOGE(TAG, "║  IMU INIT FAILED!                            ║");
        ESP_LOGE(TAG, "║                                              ║");
        ESP_LOGE(TAG, "║  Possible causes:                            ║");
        ESP_LOGE(TAG, "║  • MPU6050 not wired correctly               ║");
        ESP_LOGE(TAG, "║  • Wrong I2C address (try AD0 pin HIGH/LOW)  ║");
        ESP_LOGE(TAG, "║  • Missing pull-up resistors on SDA/SCL      ║");
        ESP_LOGE(TAG, "║  • Damaged MPU6050 module                    ║");
        ESP_LOGE(TAG, "║                                              ║");
        ESP_LOGE(TAG, "║  Check wiring:                                ║");
        ESP_LOGE(TAG, "║    SDA → GPIO %2d                             ║", I2C_SDA_PIN);
        ESP_LOGE(TAG, "║    SCL → GPIO %2d                             ║", I2C_SCL_PIN);
        ESP_LOGE(TAG, "║    VCC → 3.3V                                ║");
        ESP_LOGE(TAG, "║    GND → GND                                 ║");
        ESP_LOGE(TAG, "╚══════════════════════════════════════════════╝");

        /* Continue without IMU — WiFi and motors can still be tested */
        ESP_LOGW(TAG, "Continuing without IMU (test mode only)...");
    } else {
        ESP_LOGI(TAG, "[3/7] IMU sensor initialized ✓");
    }

    /* ── Initialize Motors ────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "[4/7] Initializing motor PWM...");
    ret = motors_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed! Cannot continue.");
        return;
    }
    ESP_LOGI(TAG, "[4/7] Motor PWM channels initialized ✓");

    /* ── Initialize WiFi AP ───────────────────────────────────────────────── */
    ESP_LOGI(TAG, "[5/7] Starting WiFi Access Point...");
    ret = wifi_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed!");
        return;
    }
    ESP_LOGI(TAG, "[5/7] WiFi AP started ✓");

    /* ── Start Web Server ─────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "[5.5/7] Starting web server...");
    ret = wifi_start_web_server();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Web server failed to start (non-critical)");
    } else {
        ESP_LOGI(TAG, "[5.5/7] Web server started ✓");
    }

    /* ── Initialize Battery Monitor ───────────────────────────────────────── */
    ESP_LOGI(TAG, "[6/7] Initializing battery monitor...");
    ret = battery_monitor_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Battery monitor init failed (non-critical)");
    } else {
        battery_monitor_start();
        ESP_LOGI(TAG, "[6/7] Battery monitor started ✓");
    }

    /* ── Initialize Flight Controller ─────────────────────────────────────── */
    ESP_LOGI(TAG, "[7/7] Initializing flight controller...");
    ret = flight_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flight controller init failed!");
        return;
    }
    flight_controller_start();
    ESP_LOGI(TAG, "[7/7] Flight controller started ✓");

    /* ── Start Status LED Task ────────────────────────────────────────────── */
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 1, NULL);

    /* ══════════════════════════════════════════════════════════════════════ */
    /*  ALL SYSTEMS GO                                                       */
    /* ══════════════════════════════════════════════════════════════════════ */
    printf("\n");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "║   ✓ ALL SYSTEMS INITIALIZED                  ║");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "║   1. Connect phone to WiFi: %s   ║", WIFI_AP_SSID);
    ESP_LOGI(TAG, "║   2. Open: http://192.168.4.1                ║");
    ESP_LOGI(TAG, "║   3. Use joysticks to control!               ║");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "║   ⚠ REMOVE PROPELLERS FOR FIRST TEST! ⚠     ║");
    ESP_LOGI(TAG, "║                                              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════╝");
    printf("\n");

    /* app_main returns, FreeRTOS scheduler takes over */
}
