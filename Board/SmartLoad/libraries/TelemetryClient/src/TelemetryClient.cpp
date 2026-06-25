#include "TelemetryClient.h"
#include <string.h>
#include <Sensors.h>
#include <LoadController.h>
#include <HttpInterface.h>

TelemetryClient::TelemetryClient() :
  serverUrl(TELEMETRY_SERVER_URL),
  sendPeriodMs(TELEMETRY_PERIOD_MS),
  retryPeriodMs(TELEMETRY_RETRY_PERIOD_MS),
  httpTimeoutMs(TELEMETRY_HTTP_TIMEOUT_MS),
  nextSendMs(0),
  lastSerialLogMs(0),
  enabled(ENABLE_HTTP_TELEMETRY),
  sensors(nullptr),
  load(nullptr),
  http(nullptr),
  snapshotMutex(nullptr),
  taskHandle(nullptr) {
  memset(&latestSnapshot, 0, sizeof(latestSnapshot));
}

String TelemetryClient::encode(String value) {
  String encoded = "";
  const char *hex = "0123456789ABCDEF";

  for (unsigned int i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value.charAt(i);

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

void TelemetryClient::begin() {
  nextSendMs = millis();
  lastSerialLogMs = 0;
}

bool TelemetryClient::begin(Sensors &sensorsRef, LoadController &loadRef, HttpInterface &httpRef) {
  begin();

  sensors = &sensorsRef;
  load = &loadRef;
  http = &httpRef;

  snapshotMutex = xSemaphoreCreateMutex();

  if (snapshotMutex == nullptr) {
    return false;
  }

  publish();

  BaseType_t result = xTaskCreatePinnedToCore(
    taskEntry,
    "SmartTelemetry",
    TELEMETRY_TASK_STACK_WORDS,
    this,
    TELEMETRY_TASK_PRIORITY,
    &taskHandle,
    TELEMETRY_TASK_CORE
  );

  return result == pdPASS;
}

void TelemetryClient::publish() {
  if (sensors == nullptr || load == nullptr || http == nullptr || snapshotMutex == nullptr) {
    return;
  }

  String timestamp = http->getTimestamp();
  const char *modeCode = load->getModeCode();

  Snapshot snapshot;
  snapshot.currentA = sensors->getCurrentA();
  snapshot.voltageV = sensors->getVoltageV();
  snapshot.powerW = sensors->getPowerW();
  snapshot.temperatureC = sensors->getTemperatureC();
  snapshot.pwm = load->getPwm();
  snapshot.elapsedSec = load->getElapsedSec();
  snapshot.isRunning = load->isRunning();
  snapshot.hasAlarm = load->hasAlarm();
  snapshot.ready = true;

  strncpy(snapshot.modeCode, modeCode, sizeof(snapshot.modeCode) - 1);
  snapshot.modeCode[sizeof(snapshot.modeCode) - 1] = '\0';
  timestamp.toCharArray(snapshot.timestamp, sizeof(snapshot.timestamp));

  if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
    latestSnapshot = snapshot;
    xSemaphoreGive(snapshotMutex);
  }
}

bool TelemetryClient::copySnapshot(Snapshot &snapshot) {
  if (snapshotMutex == nullptr) {
    return false;
  }

  if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
    return false;
  }

  snapshot = latestSnapshot;
  xSemaphoreGive(snapshotMutex);

  return snapshot.ready;
}

void TelemetryClient::taskEntry(void *parameter) {
  TelemetryClient *client = static_cast<TelemetryClient *>(parameter);

  if (client != nullptr) {
    client->runTask();
  }

  vTaskDelete(nullptr);
}

void TelemetryClient::runTask() {
  for (;;) {
    if (shouldSend()) {
      Snapshot snapshot;

      if (copySnapshot(snapshot)) {
        sendSnapshot(
          String(snapshot.timestamp),
          snapshot.isRunning,
          snapshot.hasAlarm,
          snapshot.modeCode,
          snapshot.currentA,
          snapshot.voltageV,
          snapshot.powerW,
          snapshot.temperatureC,
          snapshot.pwm,
          snapshot.elapsedSec
        );
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

bool TelemetryClient::shouldSend() {
  if (!enabled) {
    return false;
  }

  if (smartLoadTimeBefore(nextSendMs)) {
    return false;
  }

  return true;
}

void TelemetryClient::send(String line) {
  if (!shouldSend()) {
    return;
  }

  unsigned long nowMs = millis();

  if (WiFi.softAPgetStationNum() == 0) {
    nextSendMs = nowMs + retryPeriodMs;
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.setConnectTimeout(httpTimeoutMs);
  http.setTimeout(httpTimeoutMs);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("line=" + encode(line));
  http.end();

  if (code >= 200 && code < 300) {
    nextSendMs = millis() + sendPeriodMs;
    return;
  }

  nextSendMs = millis() + retryPeriodMs;
}

String TelemetryClient::buildLine(String timestamp, String eventName, String infoText, float currentA, float voltageV, float powerW, float temperatureC, int pwm, unsigned long elapsedSec, const char *modeCode) {
  String line = "";

  line += timestamp;
  line += ",";
  line += eventName;
  line += ",";
  line += String(currentA, 3);
  line += ",";
  line += String(voltageV, 2);
  line += ",";
  line += String(powerW, 1);
  line += ",";
  line += String(temperatureC, 1);
  line += ",";
  line += String(pwm);
  line += ",";
  line += String(elapsedSec);
  line += ",";
  line += modeCode;
  line += ",";
  line += infoText;

  return line;
}

void TelemetryClient::sendSnapshot(String timestamp, bool isRunning, bool hasAlarm, const char *modeCode, float currentA, float voltageV, float powerW, float temperatureC, int pwm, unsigned long elapsedSec) {
  String eventName;

  if (isRunning) {
    eventName = "DATA";
  } else if (hasAlarm) {
    eventName = "ERROR_STATE";
  } else {
    eventName = "IDLE";
  }

  String line = buildLine(timestamp, eventName, "", currentA, voltageV, powerW, temperatureC, pwm, elapsedSec, modeCode);
  send(line);

  if (smartLoadTimeBefore(lastSerialLogMs + LOG_PERIOD_MS)) {
    return;
  }

  lastSerialLogMs = millis();
  Serial.println(line);
}

