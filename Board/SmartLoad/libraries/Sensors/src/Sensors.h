#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include "config.h"

class Sensors {
private:
  uint8_t currentNPin;
  uint8_t currentPPin;
  uint8_t voltageNPin;
  uint8_t voltagePPin;

  OneWire oneWire;

  float currentScale;
  float zeroDiffRaw;
  float filterK;

  float instantCurrent;
  float instantVoltage;
  float filteredCurrent;
  float filteredVoltage;
  float measuredCurrent;
  float measuredVoltage;
  float measuredPower;
  float measuredTemp;

  bool filterIsReady;
  bool tempRequestIsStarted;
  unsigned long tempRequestStartMs;

  float voltageDividerK();
  float readAnalogAverage(uint8_t pin);
  void updateTemperature();

public:
  Sensors();

  void begin();
  bool calibrateCurrentZero();
  void update();
  void resetFilter();

  float getCurrentA();
  float getVoltageV();
  float getPowerW();
  float getTemperatureC();
  float getCurrentZeroRaw();
  float getVoltageDividerK();
};
