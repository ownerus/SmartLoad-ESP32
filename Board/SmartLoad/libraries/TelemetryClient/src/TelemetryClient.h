#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include <Sensors.h>
#include <LoadController.h>

class TelemetryClient {
private:
  const char *serverUrl;
  unsigned long sendPeriodMs;
  unsigned long httpTimeoutMs;
  unsigned long lastSendMs;
  unsigned long lastSerialLogMs;
  unsigned long pauseUntilMs;
  int failCounter;
  bool enabled;

  String encode(String value);

public:
  TelemetryClient();

  void begin();
  bool shouldSend();
  void send(String line);
  String buildLine(String timestamp, String eventName, String infoText, Sensors &sensors, LoadController &load);
  void sendPeriodic(Sensors &sensors, LoadController &load, String timestamp);
  void setEnabled(bool value);
  void setServerUrl(const char *url);
  void setPeriodMs(unsigned long value);
};
