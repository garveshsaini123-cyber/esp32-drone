/**
 * ============================================================================
 *  SkyPilot Drone - Configuration Header
 * ============================================================================
 *  All pin definitions, constants, and tuning parameters in one place.
 *  Modify this file to match YOUR hardware setup.
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ─────────────────────────────────────────────────────────────────────────── */
/*  FIRMWARE VERSION                                                          */
/* ─────────────────────────────────────────────────────────────────────────── */
#define FIRMWARE_VERSION_MAJOR  1
#define FIRMWARE_VERSION_MINOR  0
#define FIRMWARE_VERSION_PATCH  0
#define FIRMWARE_NAME           "SkyPilot-FC"

/* ─────────────────────────────────────────────────────────────────────────── */
/*  WiFi ACCESS POINT CONFIGURATION                                           */
/* ─────────────────────────────────────────────────────────────────────────── */
#define WIFI_AP_SSID            "SkyPilot-Drone"
#define WIFI_AP_PASSWORD        "drone1234"
#define WIFI_AP_CHANNEL         6
#define WIFI_AP_MAX_CONNECTIONS 2

/* ─────────────────────────────────────────────────────────────────────────── */
/*  UDP COMMUNICATION                                                         */
/* ─────────────────────────────────────────────────────────────────────────── */
#define UDP_PORT                4210
#define UDP_BUFFER_SIZE         64
#define UDP_PACKET_SIZE         8
#define UDP_PACKET_HEADER       0xAA
#define UDP_TIMEOUT_MS          500      /* Failsafe if no packet received */
#define UDP_WEB_SERVER_PORT     80       /* HTTP server for phone controller */

/* ─────────────────────────────────────────────────────────────────────────── */
/*  I2C PINS (MPU6050 IMU)                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22
#define I2C_FREQ_HZ             400000   /* 400 kHz Fast Mode */
#define I2C_PORT_NUM            0

/* ─────────────────────────────────────────────────────────────────────────── */
/*  MPU6050 SENSOR                                                            */
/* ─────────────────────────────────────────────────────────────────────────── */
#define MPU6050_ADDR            0x68
#define MPU6050_WHO_AM_I_REG    0x75
#define MPU6050_WHO_AM_I_VAL    0x68
#define MPU6050_PWR_MGMT_1      0x6B
#define MPU6050_SMPLRT_DIV      0x19
#define MPU6050_CONFIG          0x1A
#define MPU6050_GYRO_CONFIG     0x1B
#define MPU6050_ACCEL_CONFIG    0x1C
#define MPU6050_ACCEL_XOUT_H    0x3B
#define MPU6050_GYRO_XOUT_H     0x43
#define MPU6050_TEMP_OUT_H      0x41

/* Accelerometer sensitivity: ±2g = 16384 LSB/g */
#define ACCEL_SENSITIVITY       16384.0f
/* Gyroscope sensitivity: ±250°/s = 131.0 LSB/(°/s) */
#define GYRO_SENSITIVITY        131.0f

/* Complementary filter coefficient (0.0–1.0, higher = more gyro trust) */
#define COMP_FILTER_ALPHA       0.98f

/* IMU calibration samples */
#define IMU_CALIBRATION_SAMPLES 2000

/* ─────────────────────────────────────────────────────────────────────────── */
/*  MOTOR / ESC PINS (PWM)                                                    */
/* ─────────────────────────────────────────────────────────────────────────── */
/*
 *  Motor Layout (X-configuration, viewed from above):
 *
 *         FRONT
 *     M1(CCW)  M2(CW)
 *         \  /
 *          \/
 *          /\
 *         /  \
 *     M3(CW)  M4(CCW)
 *         REAR
 */
#define MOTOR1_PIN              25       /* Front-Left  (CCW) */
#define MOTOR2_PIN              26       /* Front-Right (CW)  */
#define MOTOR3_PIN              27       /* Rear-Left   (CW)  */
#define MOTOR4_PIN              14       /* Rear-Right  (CCW) */

/* PWM Configuration for ESCs */
#define ESC_PWM_FREQ_HZ         50       /* Standard servo/ESC frequency */
#define ESC_PWM_RESOLUTION      16       /* 16-bit resolution */
#define ESC_PULSE_MIN_US        1000     /* Minimum pulse width (motor off) */
#define ESC_PULSE_MAX_US        2000     /* Maximum pulse width (full throttle) */
#define ESC_ARM_PULSE_US        1000     /* Arming pulse width */
#define ESC_ARM_DELAY_MS        3000     /* Time to hold arm signal */

/* LEDC channels for each motor */
#define MOTOR1_LEDC_CHANNEL     0
#define MOTOR2_LEDC_CHANNEL     1
#define MOTOR3_LEDC_CHANNEL     2
#define MOTOR4_LEDC_CHANNEL     3
#define MOTOR_LEDC_TIMER        0

/* ─────────────────────────────────────────────────────────────────────────── */
/*  STATUS LED & BUZZER                                                       */
/* ─────────────────────────────────────────────────────────────────────────── */
#define STATUS_LED_PIN          2        /* Built-in LED on most ESP32 boards */
#define BUZZER_PIN              13       /* Optional piezo buzzer */

/* ─────────────────────────────────────────────────────────────────────────── */
/*  BATTERY MONITORING (ADC)                                                  */
/* ─────────────────────────────────────────────────────────────────────────── */
#define BATTERY_ADC_PIN         34       /* ADC1_CH6 */
#define BATTERY_VOLTAGE_DIVIDER 11.0f    /* Resistor divider ratio */
#define BATTERY_ADC_VREF        3.3f     /* ESP32 ADC reference */
#define BATTERY_ADC_MAX         4095     /* 12-bit ADC resolution */
#define BATTERY_LOW_VOLTAGE     10.5f    /* 3S LiPo critical low (3.5V/cell) */
#define BATTERY_WARN_VOLTAGE    11.1f    /* 3S LiPo warning (3.7V/cell) */
#define BATTERY_FULL_VOLTAGE    12.6f    /* 3S LiPo fully charged */

/* ─────────────────────────────────────────────────────────────────────────── */
/*  PID CONTROLLER DEFAULTS                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */
/* Roll axis */
#define PID_ROLL_KP             1.2f
#define PID_ROLL_KI             0.02f
#define PID_ROLL_KD             0.5f

/* Pitch axis */
#define PID_PITCH_KP            1.2f
#define PID_PITCH_KI            0.02f
#define PID_PITCH_KD            0.5f

/* Yaw axis */
#define PID_YAW_KP              2.0f
#define PID_YAW_KI              0.05f
#define PID_YAW_KD              0.0f

/* PID output limits */
#define PID_OUTPUT_MAX          400.0f
#define PID_OUTPUT_MIN         -400.0f
#define PID_INTEGRAL_MAX        100.0f
#define PID_INTEGRAL_MIN       -100.0f

/* ─────────────────────────────────────────────────────────────────────────── */
/*  FLIGHT CONTROLLER                                                         */
/* ─────────────────────────────────────────────────────────────────────────── */
#define FLIGHT_LOOP_FREQ_HZ     250      /* Main loop frequency (4ms period) */
#define FLIGHT_LOOP_PERIOD_US   (1000000 / FLIGHT_LOOP_FREQ_HZ)

#define THROTTLE_MIN            1000
#define THROTTLE_MAX            2000
#define THROTTLE_ARM_MAX        1050     /* Max throttle to allow arming */
#define THROTTLE_DEADBAND       50       /* Below this = motors off */

/* Maximum tilt angle in degrees */
#define MAX_ROLL_ANGLE          30.0f
#define MAX_PITCH_ANGLE         30.0f
#define MAX_YAW_RATE            180.0f   /* Degrees per second */

/* ─────────────────────────────────────────────────────────────────────────── */
/*  TASK PRIORITIES & STACK SIZES                                              */
/* ─────────────────────────────────────────────────────────────────────────── */
#define FLIGHT_TASK_PRIORITY    5        /* Highest – real-time critical */
#define WIFI_TASK_PRIORITY      3
#define BATTERY_TASK_PRIORITY   1        /* Lowest – periodic check */
#define WEB_TASK_PRIORITY       2

#define FLIGHT_TASK_STACK       8192
#define WIFI_TASK_STACK         4096
#define BATTERY_TASK_STACK      2048
#define WEB_TASK_STACK          8192

/* ─────────────────────────────────────────────────────────────────────────── */
/*  CONTROL INPUT STRUCTURE                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint16_t throttle;      /* 0–2000 */
    int16_t  roll;          /* -128 to +127 (centered at 0) */
    int16_t  pitch;         /* -128 to +127 (centered at 0) */
    int16_t  yaw;           /* -128 to +127 (centered at 0) */
    uint8_t  armed;         /* 1 = armed, 0 = disarmed */
    uint8_t  land_request;  /* 1 = auto-land requested */
    uint8_t  calibrate;     /* 1 = recalibrate IMU */
} control_input_t;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  IMU DATA STRUCTURE                                                        */
/* ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    float accel_x, accel_y, accel_z;  /* g-force */
    float gyro_x, gyro_y, gyro_z;    /* degrees/sec */
    float roll, pitch, yaw;           /* filtered angles (degrees) */
    float temperature;                /* Celsius */
} imu_data_t;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  DRONE STATE ENUM                                                          */
/* ─────────────────────────────────────────────────────────────────────────── */
typedef enum {
    STATE_INIT          = 0,
    STATE_CALIBRATING   = 1,
    STATE_IDLE          = 2,
    STATE_ARMED         = 3,
    STATE_FLYING        = 4,
    STATE_LANDING       = 5,
    STATE_FAILSAFE      = 6,
    STATE_ERROR         = 7
} drone_state_t;

#endif /* CONFIG_H */
