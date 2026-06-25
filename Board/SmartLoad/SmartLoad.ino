#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include <Sensors.h>
#include <LoadController.h>
#include <HttpInterface.h>
#include <TelemetryClient.h>

WebServer server(80);

unsigned long lastSensorMs = 0;
unsigned long lastControlMs = 0;

Sensors smartSensors;
LoadController smartLoad;
HttpInterface smartHttp;
TelemetryClient smartTelemetry;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting SmartLoad ESP32...");
  Serial.println("timestamp,event,current_A,voltage_V,power_W,temp_C,pwm,elapsed_s,mode,info");

  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  smartLoad.begin();
  smartSensors.begin();

  delay(300);

  if (smartSensors.calibrateCurrentZero()) {
    Serial.print("ZERO OK. ZeroDiff raw = ");
    Serial.println(smartSensors.getCurrentZeroRaw());
  } else {
    smartLoad.setMessage("Первичная калибровка нуля не выполнена");
    Serial.println("ZERO FAILED ON STARTUP");
  }

  smartHttp.begin(server, smartSensors, smartLoad);

  if (!smartTelemetry.begin(smartSensors, smartLoad, smartHttp)) {
    smartLoad.setMessage("Ошибка запуска задачи телеметрии");
    Serial.println("TELEMETRY TASK START FAILED");
  }

  Serial.println("System ready");
}

void loop() {
  smartHttp.update();

  if (smartLoadTimeReached(lastSensorMs + SENSOR_PERIOD_MS)) {
    lastSensorMs = millis();
    smartSensors.update();
  }

  if (smartLoadTimeReached(lastControlMs + CONTROL_PERIOD_MS)) {
    lastControlMs = millis();
    smartLoad.update(
      smartSensors.getCurrentA(),
      smartSensors.getVoltageV(),
      smartSensors.getPowerW(),
      smartSensors.getTemperatureC()
    );
  } else {
    smartLoad.updateFan(smartSensors.getTemperatureC());
  }

  smartTelemetry.publish();

  digitalWrite(LED_STATUS_PIN, smartLoad.isRunning() ? HIGH : LOW);

  delay(2);
}
