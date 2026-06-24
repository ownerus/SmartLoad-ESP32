#pragma once

// =====================================================
// SmartLoad ESP32
// Электронная нагрузка на DOIT ESP32 DevKit V1 / ESP-WROOM-32.
// =====================================================

// =====================================================
// Настройки проекта
// =====================================================

// Wi-Fi точка доступа ESP32.
#define WIFI_SSID     "ESP32_PANEL"
#define WIFI_PASSWORD "12345678"

// Отправка телеметрии на Python HTTP-сервер.
// Компьютер должен быть подключён к Wi-Fi ESP32_PANEL.
// Если у компьютера другой IP в сети ESP32, поменять адрес ниже.
#define ENABLE_HTTP_TELEMETRY 1
#define TELEMETRY_SERVER_URL "http://192.168.4.2:8000/telemetry"
#define TELEMETRY_HTTP_TIMEOUT_MS 80
#define TELEMETRY_FAIL_PAUSE_MS 5000UL

// =====================================================
// Пины по неактуальной схеме SmartLoad.
// =====================================================

#define LED_STATUS_PIN      2

#define ADC_CURRENT_N_PIN   34    // CurrentSensorN
#define ADC_CURRENT_P_PIN   35    // CurrentSensorP
#define ADC_VOLTAGE_P_PIN   32    // VoltageSensorP
#define ADC_VOLTAGE_N_PIN   34    // VoltageSensorN

#define LOAD_PWM_PIN        33    // PWM -> TLP152 -> MOSFET Q2
#define FAN_PWM_PIN         25    // PWM_FAN

#define TEMP_ONEWIRE_PIN    27    // DS18B20

// Главная защита первого запуска.
// 0 = PWM считается, но на силовой MOSFET не подаётся.
// 1 = реальный PWM подаётся на силовую нагрузку.
#define ENABLE_LOAD_OUTPUT  0

// Защита от просадки входного напряжения.
#define ENABLE_VOLTAGE_PROTECTION 1

// =====================================================
// Периоды работы.
// =====================================================

#define SENSOR_PERIOD_MS    20
#define CONTROL_PERIOD_MS   20
#define LOG_PERIOD_MS       1000
#define TELEMETRY_PERIOD_MS 100
#define CLIENT_TIME_TIMEOUT_MS 600000UL

// =====================================================
// Настройки PWM.
// =====================================================

#define PWM_FREQ_HZ             5000
#define PWM_RESOLUTION_BITS     8

#define PWM_MIN                 0
#define PWM_MAX                 255

// 0 = обычная логика, больше PWM значит больше ток.
// 1 = инвертированная логика.
#define PWM_INVERTED            0

// Ограничение скорости изменения PWM.
// Вверх медленно, вниз быстрее.
#define PWM_STEP_UP_MAX         2.0
#define PWM_STEP_DOWN_MAX       15.0

// =====================================================
// ADC, датчики и калибровка.
// =====================================================

#define ADC_REF_VOLTAGE 3.3
#define ADC_MAX_VALUE   4095.0

// Делитель напряжения по схеме: R3 = 220 kOhm, R4 = 10 kOhm.
#define VOLTAGE_R_TOP       220000.0
#define VOLTAGE_R_BOTTOM    10000.0

// Коэффициент пересчёта тока: 24.39 А/В * 3.3 В / 4095.
float currentK = 0.01965;

// PI-регулятор тока для режима I = const.
// Мягкие стартовые коэффициенты для первого запуска.
float kpI = 0.8;       // P-часть: реакция на изменение ошибки
float kiI = 1.5;       // I-часть: постепенное дотягивание тока

// PI-регулятор мощности для режима P = const.
float kpP = 0.03;
float kiP = 0.08;

// Если CurrentSensorP и CurrentSensorN фактически перепутаны, поставить 1.
#define CURRENT_DIFF_INVERTED       0

// Если VoltageSensorP и VoltageSensorN фактически перепутаны, поставить 1.
#define VOLTAGE_DIFF_INVERTED       0

#define CURRENT_DEAD_ZONE_A         0.00
#define VOLTAGE_DEAD_ZONE_V         0.00

#define CURRENT_ZERO_SAMPLES        50
#define CURRENT_ZERO_STABILITY_RAW  40.0

#define FILTER_K                    0.15

// =====================================================
// Защиты.
// =====================================================

#define OVER_CURRENT_FACTOR         1.25
#define OVER_CURRENT_CONFIRM_COUNT  3

// =====================================================
// Вентиляторы.
// =====================================================

#define FAN_ON_TEMP_C   45.0
#define FAN_OFF_TEMP_C  35.0
