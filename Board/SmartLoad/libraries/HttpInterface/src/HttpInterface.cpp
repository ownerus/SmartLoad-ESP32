#include "HttpInterface.h"

HttpInterface::HttpInterface() :
  web(nullptr),
  sensorSource(nullptr),
  loadController(nullptr),
  clientTimeValid(false),
  clientEpochMs(0),
  clientTzOffsetMin(0),
  clientTimeSyncMs(0) {
}

int64_t HttpInterface::parseInt64(const String &text) {
  int64_t value = 0;
  const int64_t limit = 900000000000000000LL;
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

    int digit = c - '0';

    if (value > (limit - digit) / 10) {
      return negative ? -limit : limit;
    }

    value = value * 10 + digit;
  }

  return negative ? -value : value;
}

void HttpInterface::updateClientTimeFromArgs() {
  if (web == nullptr || !web->hasArg("epoch")) {
    return;
  }

  int64_t epochMs = parseInt64(web->arg("epoch"));

  if (epochMs < 0 || epochMs > 4102444800000LL) {
    return;
  }

  clientEpochMs = epochMs;

  if (web->hasArg("tz")) {
    clientTzOffsetMin = web->arg("tz").toInt();

    if (clientTzOffsetMin < -840 || clientTzOffsetMin > 840) {
      clientTzOffsetMin = 0;
    }
  }

  clientTimeSyncMs = millis();
  clientTimeValid = true;
}

String HttpInterface::escapeJson(String text) {
  text.replace("\\", "\\\\");
  text.replace("\"", "\\\"");
  text.replace("\n", " ");
  text.replace("\r", " ");

  return text;
}

String HttpInterface::buildDataJson() {
  if (sensorSource == nullptr || loadController == nullptr) {
    return "{}";
  }

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

String HttpInterface::buildDebugDataJson() {
  if (sensorSource == nullptr || loadController == nullptr) {
    return "{}";
  }

  String json = "{";

  json += "\"kpI\":";
  json += String(loadController->getCurrentKp(), 4);
  json += ",";

  json += "\"kiI\":";
  json += String(loadController->getCurrentKi(), 4);
  json += ",";

  json += "\"kpP\":";
  json += String(loadController->getPowerKp(), 4);
  json += ",";

  json += "\"kiP\":";
  json += String(loadController->getPowerKi(), 4);
  json += ",";

  json += "\"stepUp\":";
  json += String(loadController->getPwmStepUpMax(), 3);
  json += ",";

  json += "\"stepDown\":";
  json += String(loadController->getPwmStepDownMax(), 3);
  json += ",";

  json += "\"fanOnTemp\":";
  json += String(loadController->getFanOnTemperature(), 1);
  json += ",";

  json += "\"maxTestCurrent\":";
  json += String(loadController->getMaxTestCurrent(), 2);
  json += ",";

  json += "\"overCurrentFactor\":";
  json += String(loadController->getOverCurrentFactor(), 3);
  json += ",";

  json += "\"overCurrentConfirmCount\":";
  json += String(loadController->getOverCurrentConfirmCount());
  json += ",";

  json += "\"loadOutputEnabled\":";
  json += loadController->isLoadOutputEnabled() ? "true" : "false";
  json += ",";

  json += "\"voltageProtectionEnabled\":";
  json += loadController->isVoltageProtectionEnabled() ? "true" : "false";
  json += ",";

  json += "\"pwmMin\":";
  json += String(loadController->getPwmMinLimit());
  json += ",";

  json += "\"pwmMax\":";
  json += String(loadController->getPwmMaxLimit());
  json += ",";

  json += "\"pwmFreq\":";
  json += String(loadController->getPwmFrequencyHz());
  json += ",";

  json += "\"pwmResolution\":";
  json += String(loadController->getPwmResolutionBits());
  json += ",";

  json += "\"currentK\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentScale(), 5) : "0";
  json += ",";

  json += "\"voltageK\":";
  json += sensorSource != nullptr ? String(sensorSource->getVoltageScale(), 5) : "0";
  json += ",";

  json += "\"adcRefVoltage\":";
  json += sensorSource != nullptr ? String(sensorSource->getAdcRefVoltage(), 4) : "0";
  json += ",";

  json += "\"adcMaxValue\":";
  json += sensorSource != nullptr ? String(sensorSource->getAdcMaxValue(), 1) : "0";
  json += ",";

  json += "\"analogAverageSamples\":";
  json += sensorSource != nullptr ? String(sensorSource->getAnalogAverageSamples()) : "0";
  json += ",";

  json += "\"movingAverageSamples\":";
  json += sensorSource != nullptr ? String(sensorSource->getMovingAverageSamples()) : "0";
  json += ",";

  json += "\"filterK\":";
  json += sensorSource != nullptr ? String(sensorSource->getFilterK(), 4) : "0";
  json += ",";

  json += "\"currentZeroSamples\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentZeroSamples()) : "0";
  json += ",";

  json += "\"currentZeroStabilityRaw\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentZeroStabilityRaw(), 1) : "0";
  json += ",";

  json += "\"debugCurrent\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentA(), 3) : "0";
  json += ",";

  json += "\"debugVoltage\":";
  json += sensorSource != nullptr ? String(sensorSource->getVoltageV(), 2) : "0";
  json += ",";

  json += "\"debugPower\":";
  json += sensorSource != nullptr ? String(sensorSource->getPowerW(), 1) : "0";
  json += ",";

  json += "\"debugTemp\":";
  json += sensorSource != nullptr ? String(sensorSource->getTemperatureC(), 1) : "0";
  json += ",";

  json += "\"currentDiffRaw\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentDiffRaw(), 1) : "0";
  json += ",";

  json += "\"currentRawP\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentRawP(), 1) : "0";
  json += ",";

  json += "\"currentRawN\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentRawN(), 1) : "0";
  json += ",";

  json += "\"currentZeroRaw\":";
  json += sensorSource != nullptr ? String(sensorSource->getCurrentZeroRaw(), 1) : "0";
  json += ",";

  json += "\"voltageDiffRaw\":";
  json += sensorSource != nullptr ? String(sensorSource->getVoltageDiffRaw(), 1) : "0";
  json += ",";

  json += "\"voltageRawP\":";
  json += sensorSource != nullptr ? String(sensorSource->getVoltageRawP(), 1) : "0";
  json += ",";

  json += "\"voltageRawN\":";
  json += sensorSource != nullptr ? String(sensorSource->getVoltageRawN(), 1) : "0";
  json += ",";

  json += "\"debugPwm\":";
  json += loadController->getPwm();
  json += ",";

  json += "\"debugMode\":\"";
  json += loadController->getModeCode();
  json += "\"";

  json += "}";

  return json;
}

void HttpInterface::sendJson() {
  if (web == nullptr || sensorSource == nullptr || loadController == nullptr) {
    return;
  }

  web->send(200, "application/json; charset=UTF-8", buildDataJson());
}

void HttpInterface::sendDebugJson() {
  if (web == nullptr || loadController == nullptr || sensorSource == nullptr) {
    return;
  }

  web->send(200, "application/json; charset=UTF-8", buildDebugDataJson());
}

void HttpInterface::handleData() {
  sendJson();
}

void HttpInterface::handleRoot() {
  if (web == nullptr) {
    return;
  }

  web->send_P(200, "text/html; charset=UTF-8", INDEX_HTML);
}

void HttpInterface::handleDebug() {
  if (web == nullptr) {
    return;
  }

  web->send_P(200, "text/html; charset=UTF-8", DEBUG_HTML);
}

void HttpInterface::handleDebugData() {
  sendDebugJson();
}

void HttpInterface::handleDebugSave() {
  if (web == nullptr || loadController == nullptr || sensorSource == nullptr) {
    return;
  }

  float kpI = web->hasArg("kpI") ? web->arg("kpI").toFloat() : loadController->getCurrentKp();
  float kiI = web->hasArg("kiI") ? web->arg("kiI").toFloat() : loadController->getCurrentKi();
  float kpP = web->hasArg("kpP") ? web->arg("kpP").toFloat() : loadController->getPowerKp();
  float kiP = web->hasArg("kiP") ? web->arg("kiP").toFloat() : loadController->getPowerKi();
  float stepUp = web->hasArg("stepUp") ? web->arg("stepUp").toFloat() : loadController->getPwmStepUpMax();
  float stepDown = web->hasArg("stepDown") ? web->arg("stepDown").toFloat() : loadController->getPwmStepDownMax();
  float fanOnTemp = web->hasArg("fanOnTemp") ? web->arg("fanOnTemp").toFloat() : loadController->getFanOnTemperature();
  float maxTestCurrent = web->hasArg("maxTestCurrent") ? web->arg("maxTestCurrent").toFloat() : loadController->getMaxTestCurrent();
  float overCurrentFactor = web->hasArg("overCurrentFactor") ? web->arg("overCurrentFactor").toFloat() : loadController->getOverCurrentFactor();
  int overCurrentConfirmCount = web->hasArg("overCurrentConfirmCount") ? web->arg("overCurrentConfirmCount").toInt() : loadController->getOverCurrentConfirmCount();
  bool loadOutputEnabled = web->hasArg("loadOutputEnabled") ? web->arg("loadOutputEnabled").toInt() == 1 : loadController->isLoadOutputEnabled();
  bool voltageProtectionEnabled = web->hasArg("voltageProtectionEnabled") ? web->arg("voltageProtectionEnabled").toInt() == 1 : loadController->isVoltageProtectionEnabled();
  int pwmMin = web->hasArg("pwmMin") ? web->arg("pwmMin").toInt() : loadController->getPwmMinLimit();
  int pwmMax = web->hasArg("pwmMax") ? web->arg("pwmMax").toInt() : loadController->getPwmMaxLimit();
  int pwmFreq = web->hasArg("pwmFreq") ? web->arg("pwmFreq").toInt() : loadController->getPwmFrequencyHz();
  int pwmResolution = web->hasArg("pwmResolution") ? web->arg("pwmResolution").toInt() : loadController->getPwmResolutionBits();
  float currentK = web->hasArg("currentK") ? web->arg("currentK").toFloat() : sensorSource->getCurrentScale();
  float voltageK = web->hasArg("voltageK") ? web->arg("voltageK").toFloat() : sensorSource->getVoltageScale();
  float adcRefVoltage = web->hasArg("adcRefVoltage") ? web->arg("adcRefVoltage").toFloat() : sensorSource->getAdcRefVoltage();
  float adcMaxValue = web->hasArg("adcMaxValue") ? web->arg("adcMaxValue").toFloat() : sensorSource->getAdcMaxValue();
  int analogAverageSamples = web->hasArg("analogAverageSamples") ? web->arg("analogAverageSamples").toInt() : sensorSource->getAnalogAverageSamples();
  int movingAverageSamples = web->hasArg("movingAverageSamples") ? web->arg("movingAverageSamples").toInt() : sensorSource->getMovingAverageSamples();
  float filterK = web->hasArg("filterK") ? web->arg("filterK").toFloat() : sensorSource->getFilterK();
  int currentZeroSamples = web->hasArg("currentZeroSamples") ? web->arg("currentZeroSamples").toInt() : sensorSource->getCurrentZeroSamples();
  float currentZeroStabilityRaw = web->hasArg("currentZeroStabilityRaw") ? web->arg("currentZeroStabilityRaw").toFloat() : sensorSource->getCurrentZeroStabilityRaw();

  loadController->setRegulatorSettings(kpI, kiI, kpP, kiP, stepUp, stepDown);
  loadController->setFanOnTemperature(fanOnTemp);
  loadController->setDebugLimits(maxTestCurrent, overCurrentFactor, overCurrentConfirmCount);
  loadController->setOutputSettings(loadOutputEnabled, voltageProtectionEnabled, pwmMin, pwmMax, pwmFreq, pwmResolution);
  sensorSource->setCurrentScale(currentK);
  sensorSource->setVoltageScale(voltageK);
  sensorSource->setAdcSettings(adcRefVoltage, adcMaxValue);
  sensorSource->setAnalogAverageSamples(analogAverageSamples);
  sensorSource->setMovingAverageSamples(movingAverageSamples);
  sensorSource->setFilterK(filterK);
  sensorSource->setCurrentZeroSettings(currentZeroSamples, currentZeroStabilityRaw);
  loadController->setMessage("Настройки PI-регулятора применены");
  sendDebugJson();
}

void HttpInterface::handleDebugReset() {
  if (web == nullptr || loadController == nullptr || sensorSource == nullptr) {
    return;
  }

  loadController->resetRegulatorSettings();
  loadController->resetOutputSettings();
  sensorSource->resetSensorSettings();
  loadController->setMessage("Настройки PI-регулятора сброшены");
  sendDebugJson();
}

void HttpInterface::handleClientTime() {
  if (web == nullptr) {
    return;
  }

  updateClientTimeFromArgs();
  web->send(200, "application/json; charset=UTF-8", "{\"ok\":true}");
}

void HttpInterface::handleStartI() {
  if (web == nullptr || loadController == nullptr || sensorSource == nullptr) {
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

  if (currentSet > loadController->getMaxTestCurrent()) {
    currentSet = loadController->getMaxTestCurrent();
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

  loadController->setMessage("Калибровка нуля тока...");

  if (!sensorSource->calibrateCurrentZero()) {
    loadController->setMessage("Старт запрещён: калибровка нуля не выполнена");
    Serial.println("START BLOCKED: AUTO ZERO FAILED");
    sendJson();
    return;
  }

  sensorSource->update();
  loadController->startIConst(currentSet, voltageMin, timeSec, temperatureMax);
  sendJson();
}

void HttpInterface::handleStartP() {
  if (web == nullptr || loadController == nullptr || sensorSource == nullptr) {
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

  if (currentMax > loadController->getMaxTestCurrent()) {
    currentMax = loadController->getMaxTestCurrent();
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

  loadController->setMessage("Калибровка нуля тока...");

  if (!sensorSource->calibrateCurrentZero()) {
    loadController->setMessage("Старт запрещён: калибровка нуля не выполнена");
    Serial.println("START BLOCKED: AUTO ZERO FAILED");
    sendJson();
    return;
  }

  sensorSource->update();
  loadController->startPConst(powerSet, currentMax, voltageMin, timeSec, temperatureMax);
  sendJson();
}

void HttpInterface::handleCommand() {
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

void HttpInterface::handleNotFound() {
  if (web == nullptr) {
    return;
  }

  web->send(404, "text/plain; charset=UTF-8", "Not found");
}

void HttpInterface::begin(WebServer &serverRef, Sensors &sensorsRef, LoadController &controllerRef) {
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

  web->on("/debug", [this]() {
    handleDebug();
  });

  web->on("/debugdata", [this]() {
    handleDebugData();
  });

  web->on("/debugsave", [this]() {
    handleDebugSave();
  });

  web->on("/debugreset", [this]() {
    handleDebugReset();
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

void HttpInterface::update() {
  if (web != nullptr) {
    web->handleClient();
  }
}

String HttpInterface::getTimestamp() {
  if (!clientTimeValid) {
    return "NO_CLIENT_TIME " + String(millis() / 1000) + "s";
  }

  if (!smartLoadTimeBefore(clientTimeSyncMs + CLIENT_TIME_TIMEOUT_MS)) {
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
