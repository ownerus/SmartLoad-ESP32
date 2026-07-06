#pragma once
#ifndef SMARTLOAD_CONFIG_H
#define SMARTLOAD_CONFIG_H

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
#define TELEMETRY_RETRY_PERIOD_MS 1000UL

// =====================================================
// Пины по схеме 2121212.pdf.
// =====================================================

#define LED_STATUS_PIN      2

#define ADC_CURRENT_P_PIN   34    // CurrentSensor_P
#define ADC_CURRENT_N_PIN   35    // CurrentSensor_N
#define ADC_VOLTAGE_P_PIN   32    // VoltageSensor_P
#define ADC_VOLTAGE_N_PIN   33    // VoltageSensor_N

#define LOAD_PWM_PIN        27    // PWM -> TLP152 -> MOSFET Q1
#define FAN_PWM_PIN         16    // PWM_FAN

#define TEMP_ONEWIRE_PIN    17    // DS18B20

// Главная защита первого запуска.
// 0 = PWM считается, но на силовой MOSFET не подаётся.
// 1 = реальный PWM подаётся на силовую нагрузку.
#define ENABLE_LOAD_OUTPUT  1

// Защита от просадки входного напряжения.
#define ENABLE_VOLTAGE_PROTECTION 1

// Защита по температуре и обязательность исправного DS18B20 для старта теста.
#define ENABLE_TEMPERATURE_PROTECTION 1

// =====================================================
// Периоды работы.
// =====================================================

#define SENSOR_PERIOD_MS    20
#define CONTROL_PERIOD_MS   20
#define CONTROL_DT_MIN_SEC  0.001
#define CONTROL_DT_MAX_SEC  0.250
#define LOG_PERIOD_MS       1000
#define TELEMETRY_PERIOD_MS 100
#define TELEMETRY_TASK_STACK_WORDS 8192
#define TELEMETRY_TASK_PRIORITY    1
#define TELEMETRY_TASK_CORE        0
#define CLIENT_TIME_TIMEOUT_MS 600000UL

inline bool smartLoadTimeBefore(unsigned long deadlineMs) {
  return (long)(deadlineMs - millis()) > 0;
}

// =====================================================
// Настройки PWM.
// =====================================================

#define PWM_FREQ_HZ             5000
#define PWM_RESOLUTION_BITS     8

#define PWM_MIN                 0
#define PWM_MAX                 255

// Ограничение скорости изменения PWM.
// Вверх медленно, вниз быстрее.
#define PWM_STEP_UP_MAX         2.0
#define PWM_STEP_DOWN_MAX       15.0

// =====================================================
// ADC, датчики и калибровка.
// =====================================================

#define ADC_REF_VOLTAGE 3.3
#define ADC_MAX_VALUE   4095.0
#define ADC_SAMPLE_PAIRS_PER_UPDATE 16
#define ADC_MOVING_AVERAGE_SAMPLES 64
#define ADC_AVERAGE_MAX_SAMPLES 512

// Токовый усилитель держит выходы около середины питания. ADC_6db дает больше counts,
// чем ADC_11db, но входы должны оставаться ниже примерно 2.2 V относительно GND.
#define CURRENT_ADC_ATTENUATION ADC_6db
#define VOLTAGE_ADC_ATTENUATION ADC_11db

// Делитель входного напряжения по схеме: R6 = 604 kOhm, R10 = 20.5 kOhm.
#define VOLTAGE_R_TOP       604000.0
#define VOLTAGE_R_BOTTOM    20500.0
#define VOLTAGE_K           ((VOLTAGE_R_TOP + VOLTAGE_R_BOTTOM) / VOLTAGE_R_BOTTOM)

// Рабочий лимит стенда на время наладки.
#define MAX_TEST_CURRENT_A 5.0

// Коэффициент пересчёта тока:
// шунт 2.5 mOhm, 75 mV при 30 A, усиление 8.2, ADC_6db ~2.2 V / 4095.
#define CURRENT_K 0.02620

// PI-регулятор тока для режима I = const.
// Мягкие стартовые коэффициенты для первого запуска.
#define KP_I 0.8       // P-часть: реакция на изменение ошибки
#define KI_I 1.5       // I-часть: постепенное дотягивание тока

// PI-регулятор мощности для режима P = const.
#define KP_P 0.03
#define KI_P 0.08

#define CURRENT_ZERO_SAMPLES        50
#define CURRENT_ZERO_STABILITY_RAW  40.0

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

#define TEMP_CONVERSION_MS     800UL
#define TEMP_VALID_TIMEOUT_MS  3000UL

#endif
