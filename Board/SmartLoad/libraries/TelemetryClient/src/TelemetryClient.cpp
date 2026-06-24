#include "TelemetryClient.h"

TelemetryClient::TelemetryClient() :
  serverUrl(TELEMETRY_SERVER_URL),
  sendPeriodMs(TELEMETRY_PERIOD_MS),
  httpTimeoutMs(TELEMETRY_HTTP_TIMEOUT_MS),
  lastSendMs(0),
  lastSerialLogMs(0),
  pauseUntilMs(0),
  failCounter(0),
  enabled(ENABLE_HTTP_TELEMETRY) {
}

String TelemetryClient::encode(String value) {
  String encoded = "";
  const char *hex = "0123456789ABCDEF";

  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
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
  lastSendMs = 0;
  lastSerialLogMs = 0;
  pauseUntilMs = 0;
  failCounter = 0;
}

bool TelemetryClient::shouldSend() {
  if (!enabled) {
    return false;
  }

  if (millis() < pauseUntilMs) {
    return false;
  }

  if (millis() - lastSendMs < sendPeriodMs) {
    return false;
  }

  return true;
}

void TelemetryClient::send(String line) {
  if (!shouldSend()) {
    return;
  }

  lastSendMs = millis();

  if (WiFi.softAPgetStationNum() == 0) {
    pauseUntilMs = millis() + TELEMETRY_FAIL_PAUSE_MS;
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(httpTimeoutMs);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("line=" + encode(line));
  http.end();

  if (code >= 200 && code < 300) {
    failCounter = 0;
    pauseUntilMs = 0;
    return;
  }

  failCounter++;
  pauseUntilMs = millis() + TELEMETRY_FAIL_PAUSE_MS;
}

String TelemetryClient::buildLine(String timestamp, String eventName, String infoText, Sensors &sensors, LoadController &load) {
  String line = "";

  line += timestamp;
  line += ",";
  line += eventName;
  line += ",";
  line += String(sensors.getCurrentA(), 3);
  line += ",";
  line += String(sensors.getVoltageV(), 2);
  line += ",";
  line += String(sensors.getPowerW(), 1);
  line += ",";
  line += String(sensors.getTemperatureC(), 1);
  line += ",";
  line += String(load.getPwm());
  line += ",";
  line += String(load.getElapsedSec());
  line += ",";
  line += load.getModeCode();
  line += ",";
  line += infoText;

  return line;
}

void TelemetryClient::sendPeriodic(Sensors &sensors, LoadController &load, String timestamp) {
  String eventName;

  if (load.isRunning()) {
    eventName = "DATA";
  } else if (load.hasAlarm()) {
    eventName = "ERROR_STATE";
  } else {
    eventName = "IDLE";
  }

  String line = buildLine(timestamp, eventName, "", sensors, load);
  send(line);

  if (millis() - lastSerialLogMs < LOG_PERIOD_MS) {
    return;
  }

  lastSerialLogMs = millis();
  Serial.println(line);
}

void TelemetryClient::setEnabled(bool value) {
  enabled = value;
}

void TelemetryClient::setServerUrl(const char *url) {
  serverUrl = url;
}

void TelemetryClient::setPeriodMs(unsigned long value) {
  sendPeriodMs = value;
}
