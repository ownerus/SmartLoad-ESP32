#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "../../config.h"
#include "../SmartLoadWeb/SmartLoadWeb.h"
#include "../Sensors/Sensors.h"
#include "../LoadController/LoadController.h"

class HttpInterface {
private:
  WebServer *web;
  Sensors *sensorSource;
  LoadController *loadController;
  bool clientTimeValid;
  int64_t clientEpochMs;
  int clientTzOffsetMin;
  unsigned long clientTimeSyncMs;

  int64_t parseInt64(const String &text) {
    int64_t value = 0;
    bool negative = false;
    int start = 0;

    if (text.length() > 0 && text[0] == '-') {
      negative = true;
      start = 1;
    }

    for (int i = start; i < text.length(); i++) {
      char c = text[i];

      if (c < '0' || c > '9') {
        break;
      }

      value = value * 10 + (c - '0');
    }

    return negative ? -value : value;
  }

  void updateClientTimeFromArgs() {
    if (web == nullptr || !web->hasArg("epoch")) {
      return;
    }

    clientEpochMs = parseInt64(web->arg("epoch"));

    if (web->hasArg("tz")) {
      clientTzOffsetMin = web->arg("tz").toInt();
    }

    clientTimeSyncMs = millis();
    clientTimeValid = true;
  }

  String escapeJson(String text) {
    text.replace("\\", "\\\\");
    text.replace("\"", "\\\"");
    text.replace("\n", " ");
    text.replace("\r", " ");

    return text;
  }

  String buildDataJson() {
    String json = "{";

    json += "\"mode\":\"";
    json += loadController->getModeCode();
    json += "\",";

    json += "\"current\":";
    json += String(sensorSource->getCurrentA(), 3);
    json += ",";

    json += "\"voltage\":";
    json += String(sensorSource->getVoltageV(), 2);
    json += ",";

    json += "\"power\":";
    json += String(sensorSource->getPowerW(), 1);
    json += ",";

    json += "\"temp\":";
    json += String(sensorSource->getTemperatureC(), 1);
    json += ",";

    json += "\"pwm\":";
    json += String(loadController->getPwm());
    json += ",";

    json += "\"elapsed\":";
    json += String(loadController->getElapsedSec());
    json += ",";

    json += "\"modeText\":\"";
    json += escapeJson(loadController->getModeText());
    json += "\",";

    json += "\"alarm\":";
    json += loadController->hasAlarm() ? "true" : "false";
    json += ",";

    json += "\"message\":\"";
    json += escapeJson(loadController->getMessage());
    json += "\"";

    json += "}";

    return json;
  }

  void sendJson() {
    if (web == nullptr || sensorSource == nullptr || loadController == nullptr) {
      return;
    }

    web->send(200, "application/json; charset=UTF-8", buildDataJson());
  }

  void handleData() {
    sendJson();
  }

  void handleRoot() {
    if (web == nullptr) {
      return;
    }

    web->send_P(200, "text/html; charset=UTF-8", INDEX_HTML);
  }

  void handleClientTime() {
    if (web == nullptr) {
      return;
    }

    updateClientTimeFromArgs();
    web->send(200, "application/json; charset=UTF-8", "{\"ok\":true}");
  }

  void handleStartI() {
    if (web == nullptr || loadController == nullptr) {
      return;
    }

    updateClientTimeFromArgs();

    float currentSet = web->arg("i").toFloat();
    float voltageMin = web->arg("vmin").toFloat();
    int timeSec = web->arg("time").toInt();
    float temperatureMax = web->arg("temp").toFloat();

    if (currentSet <= 0.0) {
      currentSet = 1.0;
    }

    if (voltageMin < 0.0) {
      voltageMin = 0.0;
    }

    if (timeSec <= 0) {
      timeSec = 300;
    }

    if (timeSec > 86400) {
      timeSec = 86400;
    }

    if (temperatureMax <= 0.0) {
      temperatureMax = 70.0;
    }

    if (!loadController->canStart()) {
      loadController->startIConst(currentSet, voltageMin, timeSec, temperatureMax);
      sendJson();
      return;
    }

    loadController->setMessage("Автокалибровка нуля тока...");

    if (!sensorSource->calibrateCurrentZero()) {
      loadController->setMessage("Старт запрещён: автокалибровка нуля не выполнена");
      Serial.println("START BLOCKED: AUTO ZERO FAILED");
      sendJson();
      return;
    }

    sensorSource->update();
    loadController->startIConst(currentSet, voltageMin, timeSec, temperatureMax);
    sendJson();
  }

  void handleStartP() {
    if (web == nullptr || loadController == nullptr) {
      return;
    }

    updateClientTimeFromArgs();

    float powerSet = web->arg("p").toFloat();
    float currentMax = web->arg("imax").toFloat();
    float voltageMin = web->arg("vmin").toFloat();
    int timeSec = web->arg("time").toInt();
    float temperatureMax = web->arg("temp").toFloat();

    if (powerSet <= 0.0) {
      powerSet = 10.0;
    }

    if (currentMax <= 0.0) {
      currentMax = 1.0;
    }

    if (voltageMin < 0.0) {
      voltageMin = 0.0;
    }

    if (timeSec <= 0) {
      timeSec = 300;
    }

    if (timeSec > 86400) {
      timeSec = 86400;
    }

    if (temperatureMax <= 0.0) {
      temperatureMax = 70.0;
    }

    if (!loadController->canStart()) {
      loadController->startPConst(powerSet, currentMax, voltageMin, timeSec, temperatureMax);
      sendJson();
      return;
    }

    loadController->setMessage("Автокалибровка нуля тока...");

    if (!sensorSource->calibrateCurrentZero()) {
      loadController->setMessage("Старт запрещён: автокалибровка нуля не выполнена");
      Serial.println("START BLOCKED: AUTO ZERO FAILED");
      sendJson();
      return;
    }

    sensorSource->update();
    loadController->startPConst(powerSet, currentMax, voltageMin, timeSec, temperatureMax);
    sendJson();
  }

  void handleCommand() {
    if (web == nullptr || loadController == nullptr) {
      return;
    }

    updateClientTimeFromArgs();

    String action = web->arg("act");

    if (action == "off") {
      loadController->stop();
    } else if (action == "estop") {
      loadController->emergencyStop();
    } else if (action == "reset") {
      loadController->resetAlarm();
    }

    sendJson();
  }

  void handleNotFound() {
    if (web == nullptr) {
      return;
    }

    web->send(404, "text/plain; charset=UTF-8", "Not found");
  }

public:
  HttpInterface() :
    web(nullptr),
    sensorSource(nullptr),
    loadController(nullptr),
    clientTimeValid(false),
    clientEpochMs(0),
    clientTzOffsetMin(0),
    clientTimeSyncMs(0) {
  }

  void begin(WebServer &serverRef, Sensors &sensorsRef, LoadController &controllerRef) {
    web = &serverRef;
    sensorSource = &sensorsRef;
    loadController = &controllerRef;

    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Wi-Fi name: ");
    Serial.println(WIFI_SSID);

    Serial.print("Wi-Fi password: ");
    Serial.println(WIFI_PASSWORD);

    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    web->on("/", [this]() {
      handleRoot();
    });

    web->on("/data", [this]() {
      handleData();
    });

    web->on("/clienttime", [this]() {
      handleClientTime();
    });

    web->on("/start", [this]() {
      handleStartI();
    });

    web->on("/startp", [this]() {
      handleStartP();
    });

    web->on("/cmd", [this]() {
      handleCommand();
    });

    web->onNotFound([this]() {
      handleNotFound();
    });

    web->begin();

    Serial.println("Web server started");
  }

  void update() {
    if (web != nullptr) {
      web->handleClient();
    }
  }

  String getTimestamp() {
    if (!clientTimeValid) {
      return "NO_CLIENT_TIME " + String(millis() / 1000) + "s";
    }

    if (millis() - clientTimeSyncMs > CLIENT_TIME_TIMEOUT_MS) {
      return "OLD_CLIENT_TIME " + String(millis() / 1000) + "s";
    }

    int64_t utcMs = clientEpochMs + (int64_t)(millis() - clientTimeSyncMs);
    int64_t localMs = utcMs + (int64_t)clientTzOffsetMin * 60000LL;

    time_t seconds = (time_t)(localMs / 1000LL);

    struct tm timeinfo;
    gmtime_r(&seconds, &timeinfo);

    char buffer[24];

    snprintf(
      buffer,
      sizeof(buffer),
      "%04d-%02d-%02d %02d:%02d:%02d",
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    );

    return String(buffer);
  }
};
