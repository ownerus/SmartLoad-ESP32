#include "LoadController.h"
#include <math.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#define LOAD_PWM_CHANNEL 0
#define FAN_PWM_CHANNEL  1

LoadController::LoadController() :
  workMode(CTRL_WAITING),
  targetCurrentA(1.0),
  targetPowerW(10.0),
  currentLimitA(1.0),
  voltageLimitV(0.0),
  temperatureLimitC(70.0),
  pwmCurrent(0.0),
  pwmOutput(0),
  currentKp(KP_I),
  currentKi(KI_I),
  powerKp(KP_P),
  powerKi(KI_P),
  pwmStepUpMax(PWM_STEP_UP_MAX),
  pwmStepDownMax(PWM_STEP_DOWN_MAX),
  fanOnTemperatureC(FAN_ON_TEMP_C),
  maxTestCurrentA(MAX_TEST_CURRENT_A),
  overCurrentFactor(OVER_CURRENT_FACTOR),
  overCurrentConfirmLimit(OVER_CURRENT_CONFIRM_COUNT),
  pwmMinLimit(PWM_MIN),
  pwmMaxLimit(PWM_MAX),
  pwmFrequencyHz(PWM_FREQ_HZ),
  pwmResolutionBits(PWM_RESOLUTION_BITS),
  loadOutputEnabled(ENABLE_LOAD_OUTPUT != 0),
  voltageProtectionEnabled(ENABLE_VOLTAGE_PROTECTION != 0),
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

bool LoadController::isValidFloat(float value) {
  return !isnan(value) && !isinf(value);
}

float LoadController::limitFloat(float value, float minValue, float maxValue) {
  if (!isValidFloat(value)) {
    return minValue;
  }

  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

int LoadController::limitDurationSec(int value) {
  if (value <= 0) {
    return 300;
  }

  if (value > 86400) {
    return 86400;
  }

  return value;
}

void LoadController::writeFanPwm(int value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(FAN_PWM_PIN, value);
#else
  ledcWrite(FAN_PWM_CHANNEL, value);
#endif
}

void LoadController::writeLoadPwm(int value) {
  if (value <= 0) {
    pwmOutput = 0;
  } else {
    pwmOutput = constrain(value, pwmMinLimit, pwmMaxLimit);
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LOAD_PWM_PIN, loadOutputEnabled ? pwmOutput : 0);
#else
  ledcWrite(LOAD_PWM_CHANNEL, loadOutputEnabled ? pwmOutput : 0);
#endif
}

float LoadController::calculateIncrementalPi(float setpoint, float measured, float kp, float ki, float &lastError, float dt) {
  if (!isValidFloat(setpoint) || !isValidFloat(measured) || !isValidFloat(kp) || !isValidFloat(ki) || !isValidFloat(dt) || dt <= 0.0) {
    lastError = 0.0;
    return 0.0;
  }

  float error = setpoint - measured;
  float deltaP = kp * (error - lastError);
  float deltaI = ki * error * dt;

  lastError = error;

  float delta = deltaP + deltaI;

  if (!isValidFloat(delta)) {
    lastError = 0.0;
    return 0.0;
  }

  if (delta > pwmStepUpMax) {
    delta = pwmStepUpMax;
  }

  if (delta < -pwmStepDownMax) {
    delta = -pwmStepDownMax;
  }

  return delta;
}

void LoadController::resetRegulators() {
  currentLastError = 0.0;
  powerLastError = 0.0;
}

void LoadController::applyPwmHardwareSettings() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LOAD_PWM_PIN, pwmFrequencyHz, pwmResolutionBits);
  ledcAttach(FAN_PWM_PIN, pwmFrequencyHz, pwmResolutionBits);
#else
  ledcSetup(LOAD_PWM_CHANNEL, pwmFrequencyHz, pwmResolutionBits);
  ledcSetup(FAN_PWM_CHANNEL, pwmFrequencyHz, pwmResolutionBits);
  ledcAttachPin(LOAD_PWM_PIN, LOAD_PWM_CHANNEL);
  ledcAttachPin(FAN_PWM_PIN, FAN_PWM_CHANNEL);
#endif
}

void LoadController::setAlarm(String text) {
  alarmIsActive = true;
  systemText = "Авария: " + text;
  workMode = CTRL_ERROR;
  stopOutput();
}

void LoadController::stopOutput() {
  pwmCurrent = 0.0;
  resetRegulators();
  writeLoadPwm(0);
}

void LoadController::begin() {
  applyPwmHardwareSettings();
  stopOutput();
  writeFanPwm(0);
}

void LoadController::update(float currentA, float voltageV, float powerW, float temperatureC, float protectionCurrentA, float dtSec) {
  if (!isRunning()) {
    updateFan(temperatureC);
    return;
  }

  if (!isValidFloat(currentA) || !isValidFloat(voltageV) || !isValidFloat(powerW) || !isValidFloat(temperatureC) || !isValidFloat(protectionCurrentA)) {
    setAlarm("SENSOR_ERROR");
    return;
  }

  dtSec = limitFloat(dtSec, CONTROL_DT_MIN_SEC, CONTROL_DT_MAX_SEC);

  unsigned long durationMs = (unsigned long)durationSec * 1000UL;

  if (durationSec > 0 && !smartLoadTimeBefore(startMs + durationMs)) {
    finishedElapsedSec = getElapsedSec();
    stopOutput();
    overCurrentConfirmCounter = 0;
    workMode = CTRL_WAITING;
    systemText = "Тест завершён по таймеру";
    updateFan(temperatureC);
    return;
  }

  if (voltageProtectionEnabled && voltageLimitV > 0.0 && voltageV < voltageLimitV) {
    setAlarm("LOW_VOLTAGE");
    return;
  }

  if (temperatureC > temperatureLimitC) {
    setAlarm("OVER_TEMPERATURE");
    updateFan(temperatureC);
    return;
  }

  if (workMode == CTRL_I_CONST) {
    if (protectionCurrentA > targetCurrentA * overCurrentFactor && protectionCurrentA > 1.0) {
      overCurrentConfirmCounter++;

      if (overCurrentConfirmCounter >= overCurrentConfirmLimit) {
        setAlarm("OVER_CURRENT");
        updateFan(temperatureC);
        return;
      }
    } else {
      overCurrentConfirmCounter = 0;
    }

    pwmCurrent += calculateIncrementalPi(targetCurrentA, currentA, currentKp, currentKi, currentLastError, dtSec);
    pwmCurrent = limitFloat(pwmCurrent, (float)pwmMinLimit, (float)pwmMaxLimit);
    writeLoadPwm((int)pwmCurrent);
  }

  if (workMode == CTRL_P_CONST) {
    if (protectionCurrentA > currentLimitA) {
      overCurrentConfirmCounter++;

      if (overCurrentConfirmCounter >= overCurrentConfirmLimit) {
        setAlarm("OVER_CURRENT");
        updateFan(temperatureC);
        return;
      }
    } else {
      overCurrentConfirmCounter = 0;
    }

    pwmCurrent += calculateIncrementalPi(targetPowerW, powerW, powerKp, powerKi, powerLastError, dtSec);
    pwmCurrent = limitFloat(pwmCurrent, (float)pwmMinLimit, (float)pwmMaxLimit);
    writeLoadPwm((int)pwmCurrent);
  }

  updateFan(temperatureC);
}

bool LoadController::startIConst(float currentSet, float voltageMin, int timeSec, float temperatureMax) {
  if (alarmIsActive || isRunning()) {
    systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
    return false;
  }

  targetCurrentA = limitFloat(currentSet, 0.1, maxTestCurrentA);
  voltageLimitV = limitFloat(voltageMin, 0.0, 1000.0);
  durationSec = limitDurationSec(timeSec);
  temperatureLimitC = limitFloat(temperatureMax, 1.0, 125.0);
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

bool LoadController::startPConst(float powerSet, float currentMax, float voltageMin, int timeSec, float temperatureMax) {
  if (alarmIsActive || isRunning()) {
    systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
    return false;
  }

  targetPowerW = limitFloat(powerSet, 1.0, 100000.0);
  currentLimitA = limitFloat(currentMax, 0.1, maxTestCurrentA);
  voltageLimitV = limitFloat(voltageMin, 0.0, 1000.0);
  durationSec = limitDurationSec(timeSec);
  temperatureLimitC = limitFloat(temperatureMax, 1.0, 125.0);
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

void LoadController::stop() {
  if (alarmIsActive) {
    systemText = "Остановка недоступна: активна авария, используйте сброс";
    return;
  }

  finishedElapsedSec = getElapsedSec();
  stopOutput();
  overCurrentConfirmCounter = 0;
  workMode = CTRL_WAITING;
  systemText = "Тест остановлен";
}

void LoadController::emergencyStop() {
  setAlarm("EMERGENCY_STOP");
}

void LoadController::resetAlarm() {
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

void LoadController::setMessage(String text) {
  systemText = text;
}

void LoadController::setRegulatorSettings(float kpI, float kiI, float kpP, float kiP, float stepUp, float stepDown) {
  currentKp = limitFloat(kpI, 0.0, 20.0);
  currentKi = limitFloat(kiI, 0.0, 50.0);
  powerKp = limitFloat(kpP, 0.0, 20.0);
  powerKi = limitFloat(kiP, 0.0, 50.0);
  pwmStepUpMax = limitFloat(stepUp, 0.01, PWM_MAX);
  pwmStepDownMax = limitFloat(stepDown, 0.01, PWM_MAX);
  resetRegulators();
}

void LoadController::setFanOnTemperature(float value) {
  fanOnTemperatureC = limitFloat(value, 0.0, 120.0);
}

void LoadController::setDebugLimits(float maxCurrent, float overCurrentFactorValue, int overCurrentConfirmCount) {
  maxTestCurrentA = limitFloat(maxCurrent, 0.1, 1000.0);
  overCurrentFactor = limitFloat(overCurrentFactorValue, 1.0, 10.0);

  if (overCurrentConfirmCount < 1) {
    overCurrentConfirmCount = 1;
  }

  if (overCurrentConfirmCount > 100) {
    overCurrentConfirmCount = 100;
  }

  overCurrentConfirmLimit = overCurrentConfirmCount;
}

void LoadController::setOutputSettings(bool outputEnabled, bool voltageProtection, int pwmMin, int pwmMax, int pwmFreq, int pwmResolution) {
  if (pwmFreq < 1) {
    pwmFreq = 1;
  }

  if (pwmFreq > 40000) {
    pwmFreq = 40000;
  }

  if (pwmResolution < 1) {
    pwmResolution = 1;
  }

  if (pwmResolution > 16) {
    pwmResolution = 16;
  }

  int pwmResolutionMax = pwmResolution >= 16 ? 65535 : ((1 << pwmResolution) - 1);

  if (pwmMin < 0) {
    pwmMin = 0;
  }

  if (pwmMin > pwmResolutionMax) {
    pwmMin = pwmResolutionMax;
  }

  if (pwmMax < 0) {
    pwmMax = 0;
  }

  if (pwmMax > pwmResolutionMax) {
    pwmMax = pwmResolutionMax;
  }

  if (pwmMax < pwmMin) {
    pwmMax = pwmMin;
  }

  loadOutputEnabled = outputEnabled;
  voltageProtectionEnabled = voltageProtection;
  pwmMinLimit = pwmMin;
  pwmMaxLimit = pwmMax;

  if (pwmFrequencyHz != pwmFreq || pwmResolutionBits != pwmResolution) {
    stopOutput();
    pwmFrequencyHz = pwmFreq;
    pwmResolutionBits = pwmResolution;
    applyPwmHardwareSettings();
  }

  writeLoadPwm(pwmOutput);
}

void LoadController::resetRegulatorSettings() {
  setRegulatorSettings(KP_I, KI_I, KP_P, KI_P, PWM_STEP_UP_MAX, PWM_STEP_DOWN_MAX);
  setFanOnTemperature(FAN_ON_TEMP_C);
  setDebugLimits(MAX_TEST_CURRENT_A, OVER_CURRENT_FACTOR, OVER_CURRENT_CONFIRM_COUNT);
}

void LoadController::resetOutputSettings() {
  setOutputSettings(
    ENABLE_LOAD_OUTPUT != 0,
    ENABLE_VOLTAGE_PROTECTION != 0,
    PWM_MIN,
    PWM_MAX,
    PWM_FREQ_HZ,
    PWM_RESOLUTION_BITS
  );
}

bool LoadController::canStart() {
  return !alarmIsActive && !isRunning();
}

void LoadController::updateFan(float temperatureC) {
  if (isRunning()) {
    fanIsActive = true;
  }

  if (temperatureC >= fanOnTemperatureC) {
    fanIsActive = true;
  }

  if (!isRunning() && temperatureC <= FAN_OFF_TEMP_C) {
    fanIsActive = false;
  }

  writeFanPwm(fanIsActive ? 255 : 0);
}

bool LoadController::isRunning() {
  return workMode == CTRL_I_CONST || workMode == CTRL_P_CONST;
}

bool LoadController::hasAlarm() {
  return alarmIsActive;
}

int LoadController::getPwm() {
  return pwmOutput;
}

unsigned long LoadController::getElapsedSec() {
  if (isRunning()) {
    return (millis() - startMs) / 1000UL;
  }

  return finishedElapsedSec;
}

String LoadController::getModeText() {
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

const char* LoadController::getModeCode() {
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

String LoadController::getMessage() {
  return systemText;
}

float LoadController::getCurrentKp() {
  return currentKp;
}

float LoadController::getCurrentKi() {
  return currentKi;
}

float LoadController::getPowerKp() {
  return powerKp;
}

float LoadController::getPowerKi() {
  return powerKi;
}

float LoadController::getPwmStepUpMax() {
  return pwmStepUpMax;
}

float LoadController::getPwmStepDownMax() {
  return pwmStepDownMax;
}

float LoadController::getFanOnTemperature() {
  return fanOnTemperatureC;
}

float LoadController::getMaxTestCurrent() {
  return maxTestCurrentA;
}

float LoadController::getOverCurrentFactor() {
  return overCurrentFactor;
}

int LoadController::getOverCurrentConfirmCount() {
  return overCurrentConfirmLimit;
}

int LoadController::getPwmMinLimit() {
  return pwmMinLimit;
}

int LoadController::getPwmMaxLimit() {
  return pwmMaxLimit;
}

int LoadController::getPwmFrequencyHz() {
  return pwmFrequencyHz;
}

int LoadController::getPwmResolutionBits() {
  return pwmResolutionBits;
}

bool LoadController::isLoadOutputEnabled() {
  return loadOutputEnabled;
}

bool LoadController::isVoltageProtectionEnabled() {
  return voltageProtectionEnabled;
}
