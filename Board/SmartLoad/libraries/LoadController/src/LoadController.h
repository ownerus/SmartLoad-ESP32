#pragma once

#include <Arduino.h>
#include "config.h"

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
  float pwmStepUpMax;
  float pwmStepDownMax;
  float fanOnTemperatureC;
  float maxTestCurrentA;
  float overCurrentFactor;
  int overCurrentConfirmLimit;
  int pwmMinLimit;
  int pwmMaxLimit;
  int pwmFrequencyHz;
  int pwmResolutionBits;
  bool loadOutputEnabled;
  bool voltageProtectionEnabled;
  bool temperatureProtectionEnabled;

  float currentLastError;
  float powerLastError;

  int durationSec;
  unsigned long startMs;
  unsigned long finishedElapsedSec;

  bool alarmIsActive;
  bool fanIsActive;
  int overCurrentConfirmCounter;
  String systemText;

  bool isValidFloat(float value);
  float limitFloat(float value, float minValue, float maxValue);
  int limitDurationSec(int value);
  void writeFanPwm(int value);
  void writeLoadPwm(int value);
  float calculateIncrementalPi(float setpoint, float measured, float kp, float ki, float &lastError, float dt);
  void resetRegulators();
  void applyPwmHardwareSettings();
  void setAlarm(String text);
  void stopOutput();

public:
  LoadController();

  void begin();
  void update(float currentA, float voltageV, float powerW, float temperatureC, bool temperatureValid, float dtSec);
  bool startIConst(float currentSet, float voltageMin, int timeSec, float temperatureMax);
  bool startPConst(float powerSet, float currentMax, float voltageMin, int timeSec, float temperatureMax);
  void stop();
  void emergencyStop();
  void resetAlarm();
  void setMessage(String text);
  void setRegulatorSettings(float kpI, float kiI, float kpP, float kiP, float stepUp, float stepDown);
  void setFanOnTemperature(float value);
  void setDebugLimits(float maxCurrent, float overCurrentFactorValue, int overCurrentConfirmCount);
  void setOutputSettings(bool outputEnabled, bool voltageProtection, bool temperatureProtection, int pwmMin, int pwmMax, int pwmFreq, int pwmResolution);
  void resetRegulatorSettings();
  void resetOutputSettings();
  bool canStart();
  void updateFan(float temperatureC);
  bool isRunning();
  bool hasAlarm();
  int getPwm();
  unsigned long getElapsedSec();
  String getModeText();
  const char* getModeCode();
  String getMessage();
  float getCurrentKp();
  float getCurrentKi();
  float getPowerKp();
  float getPowerKi();
  float getPwmStepUpMax();
  float getPwmStepDownMax();
  float getFanOnTemperature();
  float getMaxTestCurrent();
  float getOverCurrentFactor();
  int getOverCurrentConfirmCount();
  int getPwmMinLimit();
  int getPwmMaxLimit();
  int getPwmFrequencyHz();
  int getPwmResolutionBits();
  bool isLoadOutputEnabled();
  bool isVoltageProtectionEnabled();
  bool isTemperatureProtectionEnabled();
};
