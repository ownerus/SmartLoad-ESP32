#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

class Sensors;
class LoadController;
class HttpInterface;

class TelemetryClient {
private:
  struct Snapshot {
    float currentA;
    float voltageV;
    float powerW;
    float temperatureC;
    int pwm;
    unsigned long elapsedSec;
    bool isRunning;
    bool hasAlarm;
    bool ready;
    char modeCode[16];
    char timestamp[32];
  };

  const char *serverUrl;
  unsigned long sendPeriodMs;
  unsigned long retryPeriodMs;
  unsigned long httpTimeoutMs;
  unsigned long nextSendMs;
  unsigned long lastSerialLogMs;
  bool enabled;

  Sensors *sensors;
  LoadController *load;
  HttpInterface *http;
  SemaphoreHandle_t snapshotMutex;
  Snapshot latestSnapshot;
  TaskHandle_t taskHandle;

  String encode(String value);
  static void taskEntry(void *parameter);
  void runTask();
  bool copySnapshot(Snapshot &snapshot);

public:
  TelemetryClient();

  void begin();
  bool begin(Sensors &sensorsRef, LoadController &loadRef, HttpInterface &httpRef);
  void publish();
  bool shouldSend();
  void send(String line);
  String buildLine(String timestamp, String eventName, String infoText, float currentA, float voltageV, float powerW, float temperatureC, int pwm, unsigned long elapsedSec, const char *modeCode);
  void sendSnapshot(String timestamp, bool isRunning, bool hasAlarm, const char *modeCode, float currentA, float voltageV, float powerW, float temperatureC, int pwm, unsigned long elapsedSec);
};
