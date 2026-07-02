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
  float voltageScale;
  float adcRefVoltage;
  float adcMaxValue;
  int samplePairsPerUpdate;
  int movingAverageSamples;
  float zeroDiffRaw;
  float filterK;
  int currentZeroSamples;
  float currentZeroStabilityRaw;

  float currentRawPBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float currentRawNBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float currentDiffRawBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float voltageRawPBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float voltageRawNBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float voltageDiffRawBuffer[ADC_AVERAGE_MAX_SAMPLES];
  float currentRawPSum;
  float currentRawNSum;
  float currentDiffRawSum;
  float voltageRawPSum;
  float voltageRawNSum;
  float voltageDiffRawSum;
  int currentAverageIndex;
  int currentAverageCount;
  int voltageAverageIndex;
  int voltageAverageCount;

  float instantCurrent;
  float instantVoltage;
  float instantPower;
  float currentRawP;
  float currentRawN;
  float voltageRawP;
  float voltageRawN;
  float currentDiffRaw;
  float voltageDiffRaw;
  float filteredCurrent;
  float filteredVoltage;
  float filteredPower;
  float measuredCurrent;
  float measuredVoltage;
  float measuredPower;
  float measuredTemp;

  bool filterIsReady;
  bool tempRequestIsStarted;
  unsigned long tempRequestStartMs;
  unsigned long updateCounter;

  void resetMovingAverages();
  void pushCurrentRawPair(float rawP, float rawN, float diff);
  void pushVoltageRawPair(float rawP, float rawN, float diff);
  void sampleCurrentPairs(int sampleCount);
  void sampleVoltagePairs(int sampleCount);
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
  float getDisplayCurrentA();
  float getDisplayPowerW();
  float getInstantCurrentA();
  float getInstantVoltageV();
  float getInstantPowerW();
  float getTemperatureC();
  float getCurrentZeroRaw();
  float getCurrentRawP();
  float getCurrentRawN();
  float getVoltageRawP();
  float getVoltageRawN();
  float getCurrentDiffRaw();
  float getVoltageDiffRaw();
  float getCurrentScale();
  float getVoltageScale();
  float getAdcRefVoltage();
  float getAdcMaxValue();
  int getAnalogAverageSamples();
  int getMovingAverageSamples();
  float getFilterK();
  int getCurrentZeroSamples();
  float getCurrentZeroStabilityRaw();
  unsigned long getUpdateCounter();
  void setCurrentScale(float value);
  void setVoltageScale(float value);
  void setAdcSettings(float refVoltage, float maxValue);
  void setAnalogAverageSamples(int value);
  void setMovingAverageSamples(int value);
  void setFilterK(float value);
  void setCurrentZeroSettings(int samples, float stabilityRaw);
  void resetCurrentScale();
  void resetVoltageScale();
  void resetSensorSettings();
};
