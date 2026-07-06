#include "Sensors.h"

Sensors::Sensors() :
  currentNPin(ADC_CURRENT_N_PIN),
  currentPPin(ADC_CURRENT_P_PIN),
  voltageNPin(ADC_VOLTAGE_N_PIN),
  voltagePPin(ADC_VOLTAGE_P_PIN),
  oneWire(TEMP_ONEWIRE_PIN),
  currentScale(CURRENT_K),
  voltageScale(VOLTAGE_K),
  adcRefVoltage(ADC_REF_VOLTAGE),
  adcMaxValue(ADC_MAX_VALUE),
  samplePairsPerUpdate(ADC_SAMPLE_PAIRS_PER_UPDATE),
  movingAverageSamples(ADC_MOVING_AVERAGE_SAMPLES),
  zeroDiffRaw(0.0),
  currentZeroSamples(CURRENT_ZERO_SAMPLES),
  currentZeroStabilityRaw(CURRENT_ZERO_STABILITY_RAW),
  currentRawPSum(0.0),
  currentRawNSum(0.0),
  currentDiffRawSum(0.0),
  voltageRawPSum(0.0),
  voltageRawNSum(0.0),
  voltageDiffRawSum(0.0),
  currentAverageIndex(0),
  currentAverageCount(0),
  voltageAverageIndex(0),
  voltageAverageCount(0),
  currentA(0.0),
  voltageV(0.0),
  powerW(0.0),
  currentRawP(0.0),
  currentRawN(0.0),
  voltageRawP(0.0),
  voltageRawN(0.0),
  currentDiffRaw(0.0),
  voltageDiffRaw(0.0),
  measuredTemp(-125.0),
  temperatureValid(false),
  tempRequestIsStarted(false),
  tempRequestStartMs(0),
  tempLastValidMs(0),
  updateCounter(0) {
}

void Sensors::resetMovingAverages() {
  currentRawPSum = 0.0;
  currentRawNSum = 0.0;
  currentDiffRawSum = 0.0;
  voltageRawPSum = 0.0;
  voltageRawNSum = 0.0;
  voltageDiffRawSum = 0.0;
  currentAverageIndex = 0;
  currentAverageCount = 0;
  voltageAverageIndex = 0;
  voltageAverageCount = 0;
}

void Sensors::pushCurrentRawPair(float rawP, float rawN, float diff) {
  if (currentAverageCount < movingAverageSamples) {
    currentAverageCount++;
  } else {
    currentRawPSum -= currentRawPBuffer[currentAverageIndex];
    currentRawNSum -= currentRawNBuffer[currentAverageIndex];
    currentDiffRawSum -= currentDiffRawBuffer[currentAverageIndex];
  }

  currentRawPBuffer[currentAverageIndex] = rawP;
  currentRawNBuffer[currentAverageIndex] = rawN;
  currentDiffRawBuffer[currentAverageIndex] = diff;
  currentRawPSum += rawP;
  currentRawNSum += rawN;
  currentDiffRawSum += diff;
  currentAverageIndex = (currentAverageIndex + 1) % movingAverageSamples;

  currentRawP = currentRawPSum / currentAverageCount;
  currentRawN = currentRawNSum / currentAverageCount;
  currentDiffRaw = currentDiffRawSum / currentAverageCount;
}

void Sensors::pushVoltageRawPair(float rawP, float rawN, float diff) {
  if (voltageAverageCount < movingAverageSamples) {
    voltageAverageCount++;
  } else {
    voltageRawPSum -= voltageRawPBuffer[voltageAverageIndex];
    voltageRawNSum -= voltageRawNBuffer[voltageAverageIndex];
    voltageDiffRawSum -= voltageDiffRawBuffer[voltageAverageIndex];
  }

  voltageRawPBuffer[voltageAverageIndex] = rawP;
  voltageRawNBuffer[voltageAverageIndex] = rawN;
  voltageDiffRawBuffer[voltageAverageIndex] = diff;
  voltageRawPSum += rawP;
  voltageRawNSum += rawN;
  voltageDiffRawSum += diff;
  voltageAverageIndex = (voltageAverageIndex + 1) % movingAverageSamples;

  voltageRawP = voltageRawPSum / voltageAverageCount;
  voltageRawN = voltageRawNSum / voltageAverageCount;
  voltageDiffRaw = voltageDiffRawSum / voltageAverageCount;
}

void Sensors::sampleCurrentPairs(int sampleCount) {
  for (int i = 0; i < sampleCount; i++) {
    float rawP = analogRead(currentPPin);
    float rawN = analogRead(currentNPin);
    float diff = rawP - rawN;

    pushCurrentRawPair(rawP, rawN, diff);
  }
}

void Sensors::sampleVoltagePairs(int sampleCount) {
  for (int i = 0; i < sampleCount; i++) {
    float rawP = analogRead(voltagePPin);
    float rawN = analogRead(voltageNPin);
    float diff = rawP - rawN;

    pushVoltageRawPair(rawP, rawN, diff);
  }
}

void Sensors::updateTemperature() {
  unsigned long nowMs = millis();

  if (!tempRequestIsStarted) {
    if (oneWire.reset()) {
      oneWire.write(0xCC);
      oneWire.write(0x44);
      tempRequestIsStarted = true;
      tempRequestStartMs = nowMs;
    } else if (tempLastValidMs == 0 || !smartLoadTimeBefore(tempLastValidMs + TEMP_VALID_TIMEOUT_MS)) {
      temperatureValid = false;
    }

    return;
  }

  if (smartLoadTimeBefore(tempRequestStartMs + TEMP_CONVERSION_MS)) {
    return;
  }

  byte dataTemp[9];
  bool readIsValid = false;

  if (oneWire.reset()) {
    oneWire.write(0xCC);
    oneWire.write(0xBE);

    for (int i = 0; i < 9; i++) {
      dataTemp[i] = oneWire.read();
    }

    if (OneWire::crc8(dataTemp, 8) == dataTemp[8]) {
      int16_t rawTemp = (int16_t)(((uint16_t)dataTemp[1] << 8) | dataTemp[0]);
      float temp = rawTemp * 0.0625;

      if (temp >= -55.0 && temp <= 125.0) {
        measuredTemp = temp;
        temperatureValid = true;
        tempLastValidMs = nowMs;
        readIsValid = true;
      }
    }
  }

  if (!readIsValid && (tempLastValidMs == 0 || !smartLoadTimeBefore(tempLastValidMs + TEMP_VALID_TIMEOUT_MS))) {
    temperatureValid = false;
  }

  tempRequestIsStarted = false;
}

void Sensors::begin() {
  analogReadResolution(12);

  analogSetPinAttenuation(currentPPin, CURRENT_ADC_ATTENUATION);
  analogSetPinAttenuation(currentNPin, CURRENT_ADC_ATTENUATION);
  analogSetPinAttenuation(voltagePPin, VOLTAGE_ADC_ATTENUATION);
  analogSetPinAttenuation(voltageNPin, VOLTAGE_ADC_ATTENUATION);
}

bool Sensors::calibrateCurrentZero() {
  float sum = 0.0;
  float minDiff = 1000000.0;
  float maxDiff = -1000000.0;

  resetMovingAverages();

  for (int i = 0; i < currentZeroSamples; i++) {
    sampleCurrentPairs(samplePairsPerUpdate);
    float diff = currentDiffRaw;

    sum += diff;

    if (diff < minDiff) {
      minDiff = diff;
    }

    if (diff > maxDiff) {
      maxDiff = diff;
    }
  }

  float spread = maxDiff - minDiff;

  if (spread > currentZeroStabilityRaw) {
    return false;
  }

  zeroDiffRaw = sum / currentZeroSamples;
  resetMeasurements();

  return true;
}

void Sensors::update() {
  sampleCurrentPairs(samplePairsPerUpdate);
  currentA = (currentDiffRaw - zeroDiffRaw) * currentScale;

  sampleVoltagePairs(samplePairsPerUpdate);
  float adcVoltage = voltageDiffRaw * adcRefVoltage / adcMaxValue;

  voltageV = adcVoltage * voltageScale;

  if (voltageV < 0.0) {
    voltageV = 0.0;
  }

  powerW = voltageV * currentA;

  updateTemperature();
  updateCounter++;
}

void Sensors::resetMeasurements() {
  currentA = 0.0;
  voltageV = 0.0;
  powerW = 0.0;
  currentRawP = 0.0;
  currentRawN = 0.0;
  voltageRawP = 0.0;
  voltageRawN = 0.0;
  currentDiffRaw = 0.0;
  voltageDiffRaw = 0.0;
  resetMovingAverages();
}

float Sensors::getCurrentA() {
  return currentA;
}

float Sensors::getVoltageV() {
  return voltageV;
}

float Sensors::getPowerW() {
  return powerW;
}

float Sensors::getTemperatureC() {
  return measuredTemp;
}

bool Sensors::isTemperatureValid() {
  return temperatureValid;
}

float Sensors::getCurrentZeroRaw() {
  return zeroDiffRaw;
}

float Sensors::getCurrentRawP() {
  return currentRawP;
}

float Sensors::getCurrentRawN() {
  return currentRawN;
}

float Sensors::getVoltageRawP() {
  return voltageRawP;
}

float Sensors::getVoltageRawN() {
  return voltageRawN;
}

float Sensors::getCurrentDiffRaw() {
  return currentDiffRaw;
}

float Sensors::getVoltageDiffRaw() {
  return voltageDiffRaw;
}

float Sensors::getCurrentScale() {
  return currentScale;
}

float Sensors::getVoltageScale() {
  return voltageScale;
}

float Sensors::getAdcRefVoltage() {
  return adcRefVoltage;
}

float Sensors::getAdcMaxValue() {
  return adcMaxValue;
}

int Sensors::getAnalogAverageSamples() {
  return samplePairsPerUpdate;
}

int Sensors::getMovingAverageSamples() {
  return movingAverageSamples;
}

int Sensors::getCurrentZeroSamples() {
  return currentZeroSamples;
}

float Sensors::getCurrentZeroStabilityRaw() {
  return currentZeroStabilityRaw;
}

unsigned long Sensors::getUpdateCounter() {
  return updateCounter;
}

void Sensors::setCurrentScale(float value) {
  if (isnan(value) || isinf(value) || value <= 0.0 || value > 10.0) {
    return;
  }

  currentScale = value;
  resetMeasurements();
}

void Sensors::setVoltageScale(float value) {
  if (isnan(value) || isinf(value) || value <= 0.0 || value > 500.0) {
    return;
  }

  voltageScale = value;
  resetMeasurements();
}

void Sensors::setAdcSettings(float refVoltage, float maxValue) {
  if (isnan(refVoltage) || isinf(refVoltage) || refVoltage <= 0.1 || refVoltage > 5.0) {
    refVoltage = adcRefVoltage;
  }

  if (isnan(maxValue) || isinf(maxValue) || maxValue <= 1.0 || maxValue > 65535.0) {
    maxValue = adcMaxValue;
  }

  adcRefVoltage = refVoltage;
  adcMaxValue = maxValue;
  resetMeasurements();
}

void Sensors::setAnalogAverageSamples(int value) {
  if (value < 1) {
    value = 1;
  }

  if (value > ADC_AVERAGE_MAX_SAMPLES) {
    value = ADC_AVERAGE_MAX_SAMPLES;
  }

  samplePairsPerUpdate = value;
  resetMeasurements();
}

void Sensors::setMovingAverageSamples(int value) {
  if (value < 1) {
    value = 1;
  }

  if (value > ADC_AVERAGE_MAX_SAMPLES) {
    value = ADC_AVERAGE_MAX_SAMPLES;
  }

  movingAverageSamples = value;
  resetMeasurements();
}

void Sensors::setCurrentZeroSettings(int samples, float stabilityRaw) {
  if (samples < 1) {
    samples = 1;
  }

  if (samples > 500) {
    samples = 500;
  }

  if (isnan(stabilityRaw) || isinf(stabilityRaw) || stabilityRaw <= 0.0 || stabilityRaw > 4095.0) {
    stabilityRaw = currentZeroStabilityRaw;
  }

  currentZeroSamples = samples;
  currentZeroStabilityRaw = stabilityRaw;
}

void Sensors::resetCurrentScale() {
  setCurrentScale(CURRENT_K);
}

void Sensors::resetVoltageScale() {
  setVoltageScale(VOLTAGE_K);
}

void Sensors::resetSensorSettings() {
  currentScale = CURRENT_K;
  voltageScale = VOLTAGE_K;
  adcRefVoltage = ADC_REF_VOLTAGE;
  adcMaxValue = ADC_MAX_VALUE;
  samplePairsPerUpdate = ADC_SAMPLE_PAIRS_PER_UPDATE;
  movingAverageSamples = ADC_MOVING_AVERAGE_SAMPLES;
  currentZeroSamples = CURRENT_ZERO_SAMPLES;
  currentZeroStabilityRaw = CURRENT_ZERO_STABILITY_RAW;
  resetMeasurements();
}
