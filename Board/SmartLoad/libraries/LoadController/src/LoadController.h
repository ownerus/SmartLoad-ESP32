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

  float currentLastError;
  float powerLastError;

  int durationSec;
  unsigned long startMs;
  unsigned long finishedElapsedSec;

  bool alarmIsActive;
  bool fanIsActive;
  int overCurrentConfirmCounter;
  String systemText;

  float limitFloat(float value, float minValue, float maxValue);
  void writeLoadPwm(int value);
  float calculateIncrementalPi(float setpoint, float measured, float kp, float ki, float &lastError, float dt);
  void resetRegulators();
  void setAlarm(String text);
  void stopOutput();

public:
  LoadController();

  void begin();
  void update(float currentA, float voltageV, float powerW, float temperatureC);
  bool startIConst(float currentSet, float voltageMin, int timeSec, float temperatureMax);
  bool startPConst(float powerSet, float currentMax, float voltageMin, int timeSec, float temperatureMax);
  void stop();
  void emergencyStop();
  void resetAlarm();
  void setMessage(String text);
  bool canStart();
  void updateFan(float temperatureC);
  bool isRunning();
  bool hasAlarm();
  bool isFanOn();
  int getPwm();
  unsigned long getElapsedSec();
  String getModeText();
  const char* getModeCode();
  String getMessage();
};
