#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "config.h"
#include <SmartLoadWeb.h>
#include <Sensors.h>
#include <LoadController.h>

class HttpInterface {
private:
  WebServer *web;
  Sensors *sensorSource;
  LoadController *loadController;
  bool clientTimeValid;
  int64_t clientEpochMs;
  int clientTzOffsetMin;
  unsigned long clientTimeSyncMs;

  int64_t parseInt64(const String &text);
  void updateClientTimeFromArgs();
  String escapeJson(String text);
  String buildDataJson();
  String buildDebugDataJson();
  void sendJson();
  void sendDebugJson();
  void handleData();
  void handleRoot();
  void handleDebug();
  void handleDebugData();
  void handleDebugSave();
  void handleDebugReset();
  void handleClientTime();
  void handleStartI();
  void handleStartP();
  void handleCommand();
  void handleNotFound();

public:
  HttpInterface();

  void begin(WebServer &serverRef, Sensors &sensorsRef, LoadController &controllerRef);
  void update();
  String getTimestamp();
};
