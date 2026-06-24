#include "Sensors.h"

Sensors::Sensors() :
  currentNPin(ADC_CURRENT_N_PIN),
  currentPPin(ADC_CURRENT_P_PIN),
  voltageNPin(ADC_VOLTAGE_N_PIN),
  voltagePPin(ADC_VOLTAGE_P_PIN),
  oneWire(TEMP_ONEWIRE_PIN),
  currentScale(CURRENT_K),
  zeroDiffRaw(0.0),
  filterK(FILTER_K),
  instantCurrent(0.0),
  instantVoltage(0.0),
  filteredCurrent(0.0),
  filteredVoltage(0.0),
  measuredCurrent(0.0),
  measuredVoltage(0.0),
  measuredPower(0.0),
  measuredTemp(-125.0),
  filterIsReady(false),
  tempRequestIsStarted(false),
  tempRequestStartMs(0) {
}

float Sensors::voltageDividerK() {
  return (VOLTAGE_R_TOP + VOLTAGE_R_BOTTOM) / VOLTAGE_R_BOTTOM;
}

float Sensors::readAnalogAverage(uint8_t pin) {
  long sum = 0;

  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }

  return sum / 16.0;
}

void Sensors::updateTemperature() {
  if (!tempRequestIsStarted) {
    if (oneWire.reset()) {
      oneWire.write(0xCC);
      oneWire.write(0x44);
      tempRequestIsStarted = true;
      tempRequestStartMs = millis();
    }

    return;
  }

  if (millis() - tempRequestStartMs < 800) {
    return;
  }

  byte dataTemp[9];

  if (oneWire.reset()) {
    oneWire.write(0xCC);
    oneWire.write(0xBE);

    for (int i = 0; i < 9; i++) {
      dataTemp[i] = oneWire.read();
    }

    if (OneWire::crc8(dataTemp, 8) == dataTemp[8]) {
      int16_t rawTemp = (dataTemp[1] << 8) | dataTemp[0];
      float temp = rawTemp * 0.0625;

      if (temp > -55.0 && temp < 125.0) {
        measuredTemp = temp;
      }
    }
  }

  tempRequestIsStarted = false;
}

void Sensors::begin() {
  analogReadResolution(12);

  analogSetPinAttenuation(currentPPin, ADC_11db);
  analogSetPinAttenuation(currentNPin, ADC_11db);
  analogSetPinAttenuation(voltagePPin, ADC_11db);
  analogSetPinAttenuation(voltageNPin, ADC_11db);
}

bool Sensors::calibrateCurrentZero() {
  float sum = 0.0;
  float minDiff = 1000000.0;
  float maxDiff = -1000000.0;

  for (int i = 0; i < CURRENT_ZERO_SAMPLES; i++) {
    float rawP = readAnalogAverage(currentPPin);
    float rawN = readAnalogAverage(currentNPin);

    float diff = rawP - rawN;

#if CURRENT_DIFF_INVERTED
    diff = -diff;
#endif

    sum += diff;

    if (diff < minDiff) {
      minDiff = diff;
    }

    if (diff > maxDiff) {
      maxDiff = diff;
    }

    delay(10);
  }

  float spread = maxDiff - minDiff;

  if (spread > CURRENT_ZERO_STABILITY_RAW) {
    return false;
  }

  zeroDiffRaw = sum / CURRENT_ZERO_SAMPLES;
  resetFilter();

  return true;
}

void Sensors::update() {
  float rawP = readAnalogAverage(currentPPin);
  float rawN = readAnalogAverage(currentNPin);

  float currentDiffRaw = rawP - rawN;

#if CURRENT_DIFF_INVERTED
  currentDiffRaw = -currentDiffRaw;
#endif

  instantCurrent = (currentDiffRaw - zeroDiffRaw) * currentScale;

  if (instantCurrent < CURRENT_DEAD_ZONE_A) {
    instantCurrent = 0.0;
  }

  float rawVoltageP = readAnalogAverage(voltagePPin);
  float rawVoltageN = readAnalogAverage(voltageNPin);
  float voltageDiffRaw = rawVoltageP - rawVoltageN;

#if VOLTAGE_DIFF_INVERTED
  voltageDiffRaw = -voltageDiffRaw;
#endif

  float adcVoltage = voltageDiffRaw * ADC_REF_VOLTAGE / ADC_MAX_VALUE;

  instantVoltage = adcVoltage * voltageDividerK();

  if (instantVoltage < VOLTAGE_DEAD_ZONE_V) {
    instantVoltage = 0.0;
  }

  if (!filterIsReady) {
    filteredCurrent = instantCurrent;
    filteredVoltage = instantVoltage;
    filterIsReady = true;
  } else {
    filteredCurrent = filteredCurrent + filterK * (instantCurrent - filteredCurrent);
    filteredVoltage = filteredVoltage + filterK * (instantVoltage - filteredVoltage);
  }

  measuredCurrent = filteredCurrent;
  measuredVoltage = filteredVoltage;
  measuredPower = measuredVoltage * measuredCurrent;

  updateTemperature();
}

void Sensors::resetFilter() {
  instantCurrent = 0.0;
  instantVoltage = 0.0;
  filteredCurrent = 0.0;
  filteredVoltage = 0.0;
  measuredCurrent = 0.0;
  measuredVoltage = 0.0;
  measuredPower = 0.0;
  filterIsReady = false;
}

float Sensors::getCurrentA() {
  return measuredCurrent;
}

float Sensors::getVoltageV() {
  return measuredVoltage;
}

float Sensors::getPowerW() {
  return measuredPower;
}

float Sensors::getTemperatureC() {
  return measuredTemp;
}

float Sensors::getCurrentZeroRaw() {
  return zeroDiffRaw;
}

float Sensors::getVoltageDividerK() {
  return voltageDividerK();
}
