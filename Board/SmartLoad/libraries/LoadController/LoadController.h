#pragma once

#include <Arduino.h>
#include "../../config.h"

class LoadController {
private:
  enum ControllerMode {
    CTRL_WAITING,
    CTRL_I_CONST,
    CTRL_P_CONST,
    CTRL_ERROR
  };

  ControllerMode workMode;

  float targetCurrentA;
  float targetPowerW;
  float currentLimitA;
  float voltageLimitV;
  float temperatureLimitC;

  float pwmCurrent;
  int pwmOutput;

  float currentKp;
  float currentKi;
  float powerKp;
  float powerKi;

  float currentLastError;
  float powerLastError;

  int durationSec;
  unsigned long startMs;
  unsigned long finishedElapsedSec;

  bool alarmIsActive;
  bool fanIsActive;
  int overCurrentConfirmCounter;
  String systemText;

  float limitFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
      return minValue;
    }

    if (value > maxValue) {
      return maxValue;
    }

    return value;
  }

  void writeLoadPwm(int value) {
    pwmOutput = constrain(value, PWM_MIN, PWM_MAX);

    int output = pwmOutput;

#if PWM_INVERTED
    output = PWM_MAX - output;
#endif

#if ENABLE_LOAD_OUTPUT
    ledcWrite(LOAD_PWM_PIN, output);
#else
    ledcWrite(LOAD_PWM_PIN, 0);
#endif
  }

  float calculateIncrementalPi(float setpoint, float measured, float kp, float ki, float &lastError, float dt) {
    float error = setpoint - measured;
    float deltaP = kp * (error - lastError);
    float deltaI = ki * error * dt;

    lastError = error;

    float delta = deltaP + deltaI;

    if (delta > PWM_STEP_UP_MAX) {
      delta = PWM_STEP_UP_MAX;
    }

    if (delta < -PWM_STEP_DOWN_MAX) {
      delta = -PWM_STEP_DOWN_MAX;
    }

    return delta;
  }

  void resetRegulators() {
    currentLastError = 0.0;
    powerLastError = 0.0;
  }

  void setAlarm(String text) {
    alarmIsActive = true;
    systemText = "Авария: " + text;
    workMode = CTRL_ERROR;
    stopOutput();
  }

  void stopOutput() {
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
  }

public:
  LoadController() :
    workMode(CTRL_WAITING),
    targetCurrentA(1.0),
    targetPowerW(10.0),
    currentLimitA(1.0),
    voltageLimitV(0.0),
    temperatureLimitC(70.0),
    pwmCurrent(0.0),
    pwmOutput(0),
    currentKp(kpI),
    currentKi(kiI),
    powerKp(kpP),
    powerKi(kiP),
    currentLastError(0.0),
    powerLastError(0.0),
    durationSec(300),
    startMs(0),
    finishedElapsedSec(0),
    alarmIsActive(false),
    fanIsActive(false),
    overCurrentConfirmCounter(0),
    systemText("") {
  }

  void begin() {
    ledcAttach(LOAD_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttach(FAN_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    stopOutput();
    ledcWrite(FAN_PWM_PIN, 0);
  }

  void update(float currentA, float voltageV, float powerW, float temperatureC) {
    if (!isRunning()) {
      updateFan(temperatureC);
      return;
    }

    if (durationSec > 0 && getElapsedSec() >= (unsigned long)durationSec) {
      finishedElapsedSec = getElapsedSec();
      stopOutput();
      overCurrentConfirmCounter = 0;
      workMode = CTRL_WAITING;
      systemText = "Тест завершён по времени";
      updateFan(temperatureC);
      return;
    }

#if ENABLE_VOLTAGE_PROTECTION
    if (voltageLimitV > 0.0 && voltageV < voltageLimitV) {
      setAlarm("LOW_VOLTAGE");
      return;
    }
#endif

    if (temperatureC > temperatureLimitC) {
      setAlarm("OVER_TEMPERATURE");
      updateFan(temperatureC);
      return;
    }

    if (workMode == CTRL_I_CONST) {
      if (currentA > targetCurrentA * OVER_CURRENT_FACTOR && currentA > 1.0) {
        overCurrentConfirmCounter++;

        if (overCurrentConfirmCounter >= OVER_CURRENT_CONFIRM_COUNT) {
          setAlarm("OVER_CURRENT");
          updateFan(temperatureC);
          return;
        }
      } else {
        overCurrentConfirmCounter = 0;
      }

      float dt = CONTROL_PERIOD_MS / 1000.0;
      pwmCurrent += calculateIncrementalPi(targetCurrentA, currentA, currentKp, currentKi, currentLastError, dt);
      pwmCurrent = limitFloat(pwmCurrent, PWM_MIN, PWM_MAX);
      writeLoadPwm((int)pwmCurrent);
    }

    if (workMode == CTRL_P_CONST) {
      if (currentA > currentLimitA) {
        overCurrentConfirmCounter++;

        if (overCurrentConfirmCounter >= OVER_CURRENT_CONFIRM_COUNT) {
          setAlarm("OVER_CURRENT");
          updateFan(temperatureC);
          return;
        }
      } else {
        overCurrentConfirmCounter = 0;
      }

      float dt = CONTROL_PERIOD_MS / 1000.0;
      pwmCurrent += calculateIncrementalPi(targetPowerW, powerW, powerKp, powerKi, powerLastError, dt);
      pwmCurrent = limitFloat(pwmCurrent, PWM_MIN, PWM_MAX);
      writeLoadPwm((int)pwmCurrent);
    }

    updateFan(temperatureC);
  }

  bool startIConst(float currentSet, float voltageMin, int timeSec, float temperatureMax) {
    if (alarmIsActive || isRunning()) {
      systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
      return false;
    }

    targetCurrentA = currentSet;
    voltageLimitV = voltageMin;
    durationSec = timeSec;
    temperatureLimitC = temperatureMax;
    finishedElapsedSec = 0;
    startMs = millis();
    overCurrentConfirmCounter = 0;
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
    workMode = CTRL_I_CONST;
    systemText = "Тест I = const запущен";

    return true;
  }

  bool startPConst(float powerSet, float currentMax, float voltageMin, int timeSec, float temperatureMax) {
    if (alarmIsActive || isRunning()) {
      systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
      return false;
    }

    targetPowerW = powerSet;
    currentLimitA = currentMax;
    voltageLimitV = voltageMin;
    durationSec = timeSec;
    temperatureLimitC = temperatureMax;
    finishedElapsedSec = 0;
    startMs = millis();
    overCurrentConfirmCounter = 0;
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
    workMode = CTRL_P_CONST;
    systemText = "Тест P = const запущен";

    return true;
  }

  void stop() {
    if (alarmIsActive) {
      systemText = "Остановка игнорирована: активна авария, требуется сброс";
      return;
    }

    finishedElapsedSec = getElapsedSec();
    stopOutput();
    overCurrentConfirmCounter = 0;
    workMode = CTRL_WAITING;
    systemText = "Тест остановлен";
  }

  void emergencyStop() {
    setAlarm("EMERGENCY_STOP");
  }

  void resetAlarm() {
    if (!alarmIsActive) {
      return;
    }

    alarmIsActive = false;
    systemText = "Авария сброшена";
    workMode = CTRL_WAITING;
    finishedElapsedSec = 0;
    overCurrentConfirmCounter = 0;
    stopOutput();
  }

  void setMessage(String text) {
    systemText = text;
  }

  bool canStart() {
    return !alarmIsActive && !isRunning();
  }

  void updateFan(float temperatureC) {
    if (isRunning()) {
      fanIsActive = true;
    }

    if (temperatureC >= FAN_ON_TEMP_C) {
      fanIsActive = true;
    }

    if (!isRunning() && temperatureC <= FAN_OFF_TEMP_C) {
      fanIsActive = false;
    }

    ledcWrite(FAN_PWM_PIN, fanIsActive ? 255 : 0);
  }

  bool isRunning() {
    return workMode == CTRL_I_CONST || workMode == CTRL_P_CONST;
  }

  bool hasAlarm() {
    return alarmIsActive;
  }

  bool isFanOn() {
    return fanIsActive;
  }

  int getPwm() {
    return pwmOutput;
  }

  unsigned long getElapsedSec() {
    if (isRunning()) {
      return (millis() - startMs) / 1000UL;
    }

    return finishedElapsedSec;
  }

  String getModeText() {
    if (workMode == CTRL_I_CONST) {
      return "I = const";
    }

    if (workMode == CTRL_P_CONST) {
      return "P = const";
    }

    if (workMode == CTRL_ERROR) {
      return "Авария";
    }

    return "Ожидание";
  }

  const char* getModeCode() {
    if (workMode == CTRL_I_CONST) {
      return "I_CONST";
    }

    if (workMode == CTRL_P_CONST) {
      return "P_CONST";
    }

    if (workMode == CTRL_ERROR) {
      return "ERROR";
    }

    return "WAITING";
  }

  String getMessage() {
    return systemText;
  }
};
