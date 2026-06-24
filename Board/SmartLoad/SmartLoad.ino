#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <time.h>

// =====================================================
// SmartLoad ESP32
// Электронная нагрузка на DOIT ESP32 DevKit V1 / ESP-WROOM-32.
// =====================================================

// =====================================================
// Настройки проекта
// =====================================================

// Wi-Fi точка доступа ESP32.
#define WIFI_SSID     "ESP32_PANEL"
#define WIFI_PASSWORD "12345678"

// Отправка телеметрии на Python HTTP-сервер.
// Компьютер должен быть подключён к Wi-Fi ESP32_PANEL.
// Если у компьютера другой IP в сети ESP32, поменять адрес ниже.
#define ENABLE_HTTP_TELEMETRY 1
#define TELEMETRY_SERVER_URL "http://192.168.4.2:8000/telemetry"
#define TELEMETRY_HTTP_TIMEOUT_MS 80
#define TELEMETRY_FAIL_PAUSE_MS 5000UL

// =====================================================
// Пины по неактуальной схеме SmartLoad.
// =====================================================

#define LED_STATUS_PIN      2

#define ADC_CURRENT_N_PIN   34    // CurrentSensorN
#define ADC_CURRENT_P_PIN   35    // CurrentSensorP
#define ADC_VOLTAGE_P_PIN   32    // VoltageSensorP
#define ADC_VOLTAGE_N_PIN   34    // VoltageSensorN

#define LOAD_PWM_PIN        33    // PWM -> TLP152 -> MOSFET Q2
#define FAN_PWM_PIN         25    // PWM_FAN

#define TEMP_ONEWIRE_PIN    27    // DS18B20

// Главная защита первого запуска.
// 0 = PWM считается, но на силовой MOSFET не подаётся.
// 1 = реальный PWM подаётся на силовую нагрузку.
#define ENABLE_LOAD_OUTPUT  0

// Защита от просадки входного напряжения.
#define ENABLE_VOLTAGE_PROTECTION 1

// =====================================================
// Периоды работы.
// =====================================================

#define SENSOR_PERIOD_MS    20
#define CONTROL_PERIOD_MS   20
#define LOG_PERIOD_MS       1000
#define TELEMETRY_PERIOD_MS 100
#define CLIENT_TIME_TIMEOUT_MS 600000UL

// =====================================================
// Настройки PWM.
// =====================================================

#define PWM_FREQ_HZ             5000
#define PWM_RESOLUTION_BITS     8

#define PWM_MIN                 0
#define PWM_MAX                 255

// 0 = обычная логика, больше PWM значит больше ток.
// 1 = инвертированная логика.
#define PWM_INVERTED            0

// Ограничение скорости изменения PWM.
// Вверх медленно, вниз быстрее.
#define PWM_STEP_UP_MAX         2.0
#define PWM_STEP_DOWN_MAX       15.0

// =====================================================
// ADC, датчики и калибровка.
// =====================================================

#define ADC_REF_VOLTAGE 3.3
#define ADC_MAX_VALUE   4095.0

// Делитель напряжения по схеме: R3 = 220 kOhm, R4 = 10 kOhm.
#define VOLTAGE_R_TOP       220000.0
#define VOLTAGE_R_BOTTOM    10000.0

// Коэффициент пересчёта тока: 24.39 А/В * 3.3 В / 4095.
float currentK = 0.01965;

// PI-регулятор тока для режима I = const.
// Мягкие стартовые коэффициенты для первого запуска.
float kpI = 0.8;       // P-часть: реакция на изменение ошибки
float kiI = 1.5;       // I-часть: постепенное дотягивание тока

// PI-регулятор мощности для режима P = const.
float kpP = 0.03;
float kiP = 0.08;

// Если CurrentSensorP и CurrentSensorN фактически перепутаны, поставить 1.
#define CURRENT_DIFF_INVERTED       0

// Если VoltageSensorP и VoltageSensorN фактически перепутаны, поставить 1.
#define VOLTAGE_DIFF_INVERTED       0

#define CURRENT_DEAD_ZONE_A         0.00
#define VOLTAGE_DEAD_ZONE_V         0.00

#define CURRENT_ZERO_SAMPLES        50
#define CURRENT_ZERO_STABILITY_RAW  40.0

#define FILTER_K                    0.15

// =====================================================
// Защиты.
// =====================================================

#define OVER_CURRENT_FACTOR         1.25
#define OVER_CURRENT_CONFIRM_COUNT  3

// =====================================================
// Вентиляторы.
// =====================================================

#define FAN_ON_TEMP_C   45.0
#define FAN_OFF_TEMP_C  35.0

WebServer server(80);

// =====================================================
// Таймеры основного цикла.
// =====================================================

unsigned long lastSensorMs = 0;
unsigned long lastControlMs = 0;

// =====================================================
// HTML веб-интерфейса
// =====================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SMARTLOAD ESP32</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#11161d;color:#eef2f7;font-family:Arial,sans-serif;padding:14px}
.wrapper{width:100%;max-width:760px;margin:0 auto}
.card{background:#1a212b;border:1px solid #2d3746;border-radius:18px;padding:16px;margin-bottom:14px;box-shadow:0 8px 24px rgba(0,0,0,.25)}
.header{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.title{font-size:30px;font-weight:800;letter-spacing:.5px}
.status{padding:8px 14px;border-radius:999px;font-size:15px;font-weight:700}
.status-waiting{background:#2e3847;color:#d7e1ef}
.status-run{background:#163b24;color:#65ef98}
.status-error{background:#4b1717;color:#ff7c7c}
.message{display:none!important;margin-top:12px;background:#121820;border:1px solid #334054;border-radius:14px;padding:12px;color:#dbe5f4;font-size:15px;line-height:1.35}
.section-title{font-size:22px;font-weight:800;margin-bottom:14px;color:#dfe8f7}
.measure-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.measure-item{background:#121820;border:1px solid #2b3443;border-radius:14px;padding:14px}
.measure-label{font-size:14px;color:#98a6b8;margin-bottom:8px}
.measure-value{font-size:28px;font-weight:800;color:#fff}
.small-value{font-size:22px}
.mode-list{display:flex;flex-direction:column;gap:12px}
.mode-item{background:#121820;border:1px solid #2b3443;border-radius:14px;padding:14px}
.mode-top{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:10px}
.mode-name{font-size:20px;font-weight:800}
.mode-code{font-size:16px;color:#9eacc0}
.mode-desc{font-size:15px;color:#c7d2e1;line-height:1.45;margin-bottom:12px}
button{width:100%;border:none;border-radius:14px;padding:14px 12px;font-size:18px;font-weight:800;cursor:pointer;color:#fff}
button:active{transform:scale(.99)}
button:disabled{cursor:not-allowed;transform:none;opacity:.62}
.btn-main{background:#188a47}
.btn-neutral{background:#596273}
.btn-danger{background:#c22b2b}
.btn-reset{background:#2a7c37}
.action-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.footer-note{text-align:center;color:#3a4452;font-size:13px;margin-top:10px;user-select:none}
.modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.72);align-items:center;justify-content:center;padding:16px;z-index:20}
.modal{width:100%;max-width:460px;background:#1a212b;border:1px solid #334054;border-radius:18px;padding:18px}
.modal-title{font-size:24px;font-weight:800;margin-bottom:16px}
.form-row{margin-bottom:12px}
.form-row label{display:block;margin-bottom:6px;color:#9aa8bb;font-size:14px}
input{width:100%;padding:13px;border-radius:12px;border:1px solid #354153;background:#121820;color:#fff;font-size:18px}
.modal-actions{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:16px}
.confirm-text{color:#dbe5f4;line-height:1.5;margin-bottom:16px;font-size:16px}
@media(max-width:640px){
  .title{font-size:26px}
  .measure-grid{grid-template-columns:1fr}
  .action-grid{grid-template-columns:1fr}
  .modal-actions{grid-template-columns:1fr}
  .measure-value{font-size:26px}
  .small-value{font-size:22px}
  .mode-name{font-size:18px}
}
</style>
</head>

<body>
<div class="wrapper">

  <div class="card">
    <div class="header">
      <div class="title">SMARTLOAD ESP32</div>
      <div id="statusBadge" class="status status-waiting">Ожидание</div>
    </div>
    <div id="messageBox" class="message"></div>
  </div>

  <div class="card">
    <div class="section-title">Измерения</div>

    <div class="measure-grid">
      <div class="measure-item">
        <div class="measure-label">Ток нагрузки</div>
        <div class="measure-value" id="current">0.00 А</div>
      </div>

      <div class="measure-item">
        <div class="measure-label">Напряжение на входе</div>
        <div class="measure-value" id="voltage">0.00 В</div>
      </div>

      <div class="measure-item">
        <div class="measure-label">Мощность</div>
        <div class="measure-value" id="power">0.0 Вт</div>
      </div>

      <div class="measure-item">
        <div class="measure-label">Температура</div>
        <div class="measure-value" id="temp">0.0 °C</div>
      </div>

    </div>
  </div>

  <div class="card">
    <div class="section-title">Режимы работы</div>

    <div class="mode-list">
      <div class="mode-item">
        <div class="mode-top">
          <div class="mode-name">Стабилизация тока</div>
          <div class="mode-code">I = const</div>
        </div>
        <div class="mode-desc">
          Настройка тока, минимального напряжения, времени испытания и температуры.
        </div>
        <button class="btn-main" onclick="openSettings()">Настроить</button>
      </div>

      <div class="mode-item">
        <div class="mode-top">
          <div class="mode-name">Стабилизация мощности</div>
          <div class="mode-code">P = const</div>
        </div>
        <div class="mode-desc">
          Настройка мощности, максимального тока, минимального напряжения, времени испытания и температуры.
        </div>
        <button class="btn-main" onclick="openPowerSettings()">Настроить</button>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Управление</div>
    <div class="action-grid">
      <button class="btn-neutral" onclick="sendCommand('off')">Остановить</button>
      <button class="btn-danger" onclick="openEmergencyConfirm()">Аварийный стоп</button>
      <button class="btn-reset" onclick="sendCommand('reset')">Сброс аварии</button>
    </div>
  </div>

</div>

<div class="modal-bg" id="settingsModal">
  <div class="modal">
    <div class="modal-title">Настройки I = const</div>

    <div class="form-row">
      <label>I, А</label>
      <input id="currentInput" type="number" step="0.1" min="0.1" value="10.0">
    </div>

    <div class="form-row">
      <label>Vmin, В</label>
      <input id="vminInput" type="number" step="0.1" min="0" value="42.0">
    </div>

    <div class="form-row">
      <label>t, с</label>
      <input id="timeInput" type="number" step="1" min="1" max="86400" value="300">
    </div>

    <div class="form-row">
      <label>Температура, °C</label>
      <input id="tempInput" type="number" step="1" min="1" value="70">
    </div>

    <div class="modal-actions">
      <button id="startIButton" class="btn-main" onclick="startIConst()">Старт</button>
      <button class="btn-neutral" onclick="closeSettings()">Отмена</button>
    </div>
  </div>
</div>

<div class="modal-bg" id="powerSettingsModal">
  <div class="modal">
    <div class="modal-title">Настройки P = const</div>

    <div class="form-row">
      <label>P, Вт</label>
      <input id="powerInput" type="number" step="1" min="1" value="100">
    </div>

    <div class="form-row">
      <label>Imax, А</label>
      <input id="imaxInput" type="number" step="0.1" min="0.1" value="10.0">
    </div>

    <div class="form-row">
      <label>Vmin, В</label>
      <input id="pVminInput" type="number" step="0.1" min="0" value="42.0">
    </div>

    <div class="form-row">
      <label>t, с</label>
      <input id="pTimeInput" type="number" step="1" min="1" max="86400" value="300">
    </div>

    <div class="form-row">
      <label>Температура, °C</label>
      <input id="pTempInput" type="number" step="1" min="1" value="70">
    </div>

    <div class="modal-actions">
      <button id="startPButton" class="btn-main" onclick="startPConst()">Старт</button>
      <button class="btn-neutral" onclick="closePowerSettings()">Отмена</button>
    </div>
  </div>
</div>

<div class="modal-bg" id="emergencyModal">
  <div class="modal">
    <div class="modal-title">Подтверждение аварийного стопа</div>
    <div class="confirm-text">
      Аварийный стоп немедленно отключит нагрузку и переведёт стенд в аварийное состояние.
    </div>
    <div class="modal-actions">
      <button id="confirmEmergencyButton" class="btn-danger" onclick="confirmEmergency()">Подтвердить</button>
      <button class="btn-neutral" onclick="closeEmergencyConfirm()">Отмена</button>
    </div>
  </div>
</div>

<script>
function setText(id, value){
  document.getElementById(id).innerText = value;
}

function timeParams(){
  let epoch = Date.now();
  let tz = -new Date().getTimezoneOffset();
  return 'epoch=' + epoch + '&tz=' + tz;
}

function updatePage(data){
  setText('current', Number(data.current).toFixed(2) + ' А');
  setText('voltage', Number(data.voltage).toFixed(2) + ' В');
  setText('power', Number(data.power).toFixed(1) + ' Вт');
  setText('temp', Number(data.temp).toFixed(1) + ' °C');

  let badge = document.getElementById('statusBadge');
  badge.innerText = data.modeText;
  badge.className = 'status ';

  if (data.alarm) {
    badge.innerText = 'Авария';
    badge.className += 'status-error';
  }
  else if (data.mode === 'ERROR') {
    badge.className += 'status-error';
  }
  else if (data.mode === 'I_CONST' || data.mode === 'P_CONST') {
    badge.className += 'status-run';
  }
  else {
    badge.className += 'status-waiting';
  }

  let msg = document.getElementById('messageBox');

  if (data.message && data.message.length > 0) {
    msg.innerText = data.message;
    msg.style.display = 'block';
  } else {
    msg.style.display = 'none';
  }
}

function syncClientTime(){
  fetch('/clienttime?' + timeParams())
    .catch(e => console.log(e));
}

function getData(){
  fetch('/data')
    .then(r => r.json())
    .then(d => updatePage(d))
    .catch(e => console.log(e));
}

function sendCommand(action){
  fetch('/cmd?act=' + action + '&' + timeParams())
    .then(r => r.json())
    .then(d => updatePage(d))
    .catch(e => console.log(e));
}

let startRequestBusy = false;

function setButtonBusy(id, busy, text){
  let button = document.getElementById(id);
  if (!button) {
    return;
  }

  if (busy) {
    button.dataset.oldText = button.innerText;
    button.innerText = text;
    button.disabled = true;
  } else {
    button.innerText = button.dataset.oldText || button.innerText;
    button.disabled = false;
  }
}

function openSettings(){
  if (startRequestBusy) {
    return;
  }

  document.getElementById('settingsModal').style.display = 'flex';
}

function closeSettings(){
  document.getElementById('settingsModal').style.display = 'none';
}

function openPowerSettings(){
  if (startRequestBusy) {
    return;
  }

  document.getElementById('powerSettingsModal').style.display = 'flex';
}

function closePowerSettings(){
  document.getElementById('powerSettingsModal').style.display = 'none';
}

function startIConst(){
  if (startRequestBusy) {
    return;
  }

  startRequestBusy = true;
  setButtonBusy('startIButton', true, 'Запуск...');

  let i = document.getElementById('currentInput').value;
  let vmin = document.getElementById('vminInput').value;
  let t = document.getElementById('timeInput').value;
  let temp = document.getElementById('tempInput').value;

  let url = '/start?i=' + i +
            '&vmin=' + vmin +
            '&time=' + t +
            '&temp=' + temp +
            '&' + timeParams();

  closeSettings();

  fetch(url)
    .then(r => r.json())
    .then(d => updatePage(d))
    .catch(e => console.log(e))
    .finally(() => {
      startRequestBusy = false;
      setButtonBusy('startIButton', false);
    });
}

function startPConst(){
  if (startRequestBusy) {
    return;
  }

  startRequestBusy = true;
  setButtonBusy('startPButton', true, 'Запуск...');

  let p = document.getElementById('powerInput').value;
  let imax = document.getElementById('imaxInput').value;
  let vmin = document.getElementById('pVminInput').value;
  let t = document.getElementById('pTimeInput').value;
  let temp = document.getElementById('pTempInput').value;

  let url = '/startp?p=' + p +
            '&imax=' + imax +
            '&vmin=' + vmin +
            '&time=' + t +
            '&temp=' + temp +
            '&' + timeParams();

  closePowerSettings();

  fetch(url)
    .then(r => r.json())
    .then(d => updatePage(d))
    .catch(e => console.log(e))
    .finally(() => {
      startRequestBusy = false;
      setButtonBusy('startPButton', false);
    });
}

function openEmergencyConfirm(){
  document.getElementById('emergencyModal').style.display = 'flex';
}

function closeEmergencyConfirm(){
  document.getElementById('emergencyModal').style.display = 'none';
}

function confirmEmergency(){
  setButtonBusy('confirmEmergencyButton', true, 'Остановка...');

  fetch('/cmd?act=estop&' + timeParams())
    .then(r => r.json())
    .then(d => {
      updatePage(d);
      closeEmergencyConfirm();
    })
    .catch(e => console.log(e))
    .finally(() => setButtonBusy('confirmEmergencyButton', false));
}

setInterval(getData, 1000);
setInterval(syncClientTime, 10000);

window.onload = function(){
  syncClientTime();
  getData();
};
</script>
</body>
</html>
)rawliteral";

class Sensors {
private:
  uint8_t currentNPin;
  uint8_t currentPPin;
  uint8_t voltageNPin;
  uint8_t voltagePPin;

  OneWire oneWire;

  float currentScale;
  float zeroDiffRaw;
  float filterK;

  float instantCurrent;
  float instantVoltage;
  float filteredCurrent;
  float filteredVoltage;
  float measuredCurrent;
  float measuredVoltage;
  float measuredPower;
  float measuredTemp;

  bool filterIsReady;
  bool tempRequestIsStarted;
  unsigned long tempRequestStartMs;

  float voltageDividerK() {
    return (VOLTAGE_R_TOP + VOLTAGE_R_BOTTOM) / VOLTAGE_R_BOTTOM;
  }

  float readAnalogAverage(uint8_t pin) {
    long sum = 0;

    for (int i = 0; i < 16; i++) {
      sum += analogRead(pin);
      delayMicroseconds(50);
    }

    return sum / 16.0;
  }

  void updateTemperature() {
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

public:
  Sensors() :
    currentNPin(ADC_CURRENT_N_PIN),
    currentPPin(ADC_CURRENT_P_PIN),
    voltageNPin(ADC_VOLTAGE_N_PIN),
    voltagePPin(ADC_VOLTAGE_P_PIN),
    oneWire(TEMP_ONEWIRE_PIN),
    currentScale(currentK),
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

  void begin() {
    analogReadResolution(12);

    analogSetPinAttenuation(currentPPin, ADC_11db);
    analogSetPinAttenuation(currentNPin, ADC_11db);
    analogSetPinAttenuation(voltagePPin, ADC_11db);
    analogSetPinAttenuation(voltageNPin, ADC_11db);
  }

  bool calibrateCurrentZero() {
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

  void update() {
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

  void resetFilter() {
    instantCurrent = 0.0;
    instantVoltage = 0.0;
    filteredCurrent = 0.0;
    filteredVoltage = 0.0;
    measuredCurrent = 0.0;
    measuredVoltage = 0.0;
    measuredPower = 0.0;
    filterIsReady = false;
  }

  float getCurrentA() {
    return measuredCurrent;
  }

  float getVoltageV() {
    return measuredVoltage;
  }

  float getPowerW() {
    return measuredPower;
  }

  float getTemperatureC() {
    return measuredTemp;
  }

  float getCurrentZeroRaw() {
    return zeroDiffRaw;
  }

  float getVoltageDividerK() {
    return voltageDividerK();
  }
};

class LoadController {
private:
  enum ControllerMode {
    CTRL_WAITING,
    CTRL_I_CONST,
    CTRL_P_CONST,
    CTRL_ERROR
  };

  ControllerMode workMode;

  float targetCurrentA;
  float targetPowerW;
  float currentLimitA;
  float voltageLimitV;
  float temperatureLimitC;

  float pwmCurrent;
  int pwmOutput;

  float currentKp;
  float currentKi;
  float powerKp;
  float powerKi;

  float currentLastError;
  float powerLastError;

  int durationSec;
  unsigned long startMs;
  unsigned long finishedElapsedSec;

  bool alarmIsActive;
  bool fanIsActive;
  int overCurrentConfirmCounter;
  String systemText;

  float limitFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
      return minValue;
    }

    if (value > maxValue) {
      return maxValue;
    }

    return value;
  }

  void writeLoadPwm(int value) {
    pwmOutput = constrain(value, PWM_MIN, PWM_MAX);

    int output = pwmOutput;

#if PWM_INVERTED
    output = PWM_MAX - output;
#endif

#if ENABLE_LOAD_OUTPUT
    ledcWrite(LOAD_PWM_PIN, output);
#else
    ledcWrite(LOAD_PWM_PIN, 0);
#endif
  }

  float calculateIncrementalPi(float setpoint, float measured, float kp, float ki, float &lastError, float dt) {
    float error = setpoint - measured;
    float deltaP = kp * (error - lastError);
    float deltaI = ki * error * dt;

    lastError = error;

    float delta = deltaP + deltaI;

    if (delta > PWM_STEP_UP_MAX) {
      delta = PWM_STEP_UP_MAX;
    }

    if (delta < -PWM_STEP_DOWN_MAX) {
      delta = -PWM_STEP_DOWN_MAX;
    }

    return delta;
  }

  void resetRegulators() {
    currentLastError = 0.0;
    powerLastError = 0.0;
  }

  void setAlarm(String text) {
    alarmIsActive = true;
    systemText = "Авария: " + text;
    workMode = CTRL_ERROR;
    stopOutput();
  }

  void stopOutput() {
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
  }

public:
  LoadController() :
    workMode(CTRL_WAITING),
    targetCurrentA(1.0),
    targetPowerW(10.0),
    currentLimitA(1.0),
    voltageLimitV(0.0),
    temperatureLimitC(70.0),
    pwmCurrent(0.0),
    pwmOutput(0),
    currentKp(kpI),
    currentKi(kiI),
    powerKp(kpP),
    powerKi(kiP),
    currentLastError(0.0),
    powerLastError(0.0),
    durationSec(300),
    startMs(0),
    finishedElapsedSec(0),
    alarmIsActive(false),
    fanIsActive(false),
    overCurrentConfirmCounter(0),
    systemText("") {
  }

  void begin() {
    ledcAttach(LOAD_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttach(FAN_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    stopOutput();
    ledcWrite(FAN_PWM_PIN, 0);
  }

  void update(float currentA, float voltageV, float powerW, float temperatureC) {
    if (!isRunning()) {
      updateFan(temperatureC);
      return;
    }

    if (durationSec > 0 && getElapsedSec() >= (unsigned long)durationSec) {
      finishedElapsedSec = getElapsedSec();
      stopOutput();
      overCurrentConfirmCounter = 0;
      workMode = CTRL_WAITING;
      systemText = "Тест завершён по времени";
      updateFan(temperatureC);
      return;
    }

#if ENABLE_VOLTAGE_PROTECTION
    if (voltageLimitV > 0.0 && voltageV < voltageLimitV) {
      setAlarm("LOW_VOLTAGE");
      return;
    }
#endif

    if (temperatureC > temperatureLimitC) {
      setAlarm("OVER_TEMPERATURE");
      updateFan(temperatureC);
      return;
    }

    if (workMode == CTRL_I_CONST) {
      if (currentA > targetCurrentA * OVER_CURRENT_FACTOR && currentA > 1.0) {
        overCurrentConfirmCounter++;

        if (overCurrentConfirmCounter >= OVER_CURRENT_CONFIRM_COUNT) {
          setAlarm("OVER_CURRENT");
          updateFan(temperatureC);
          return;
        }
      } else {
        overCurrentConfirmCounter = 0;
      }

      float dt = CONTROL_PERIOD_MS / 1000.0;
      pwmCurrent += calculateIncrementalPi(targetCurrentA, currentA, currentKp, currentKi, currentLastError, dt);
      pwmCurrent = limitFloat(pwmCurrent, PWM_MIN, PWM_MAX);
      writeLoadPwm((int)pwmCurrent);
    }

    if (workMode == CTRL_P_CONST) {
      if (currentA > currentLimitA) {
        overCurrentConfirmCounter++;

        if (overCurrentConfirmCounter >= OVER_CURRENT_CONFIRM_COUNT) {
          setAlarm("OVER_CURRENT");
          updateFan(temperatureC);
          return;
        }
      } else {
        overCurrentConfirmCounter = 0;
      }

      float dt = CONTROL_PERIOD_MS / 1000.0;
      pwmCurrent += calculateIncrementalPi(targetPowerW, powerW, powerKp, powerKi, powerLastError, dt);
      pwmCurrent = limitFloat(pwmCurrent, PWM_MIN, PWM_MAX);
      writeLoadPwm((int)pwmCurrent);
    }

    updateFan(temperatureC);
  }

  bool startIConst(float currentSet, float voltageMin, int timeSec, float temperatureMax) {
    if (alarmIsActive || isRunning()) {
      systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
      return false;
    }

    targetCurrentA = currentSet;
    voltageLimitV = voltageMin;
    durationSec = timeSec;
    temperatureLimitC = temperatureMax;
    finishedElapsedSec = 0;
    startMs = millis();
    overCurrentConfirmCounter = 0;
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
    workMode = CTRL_I_CONST;
    systemText = "Тест I = const запущен";

    return true;
  }

  bool startPConst(float powerSet, float currentMax, float voltageMin, int timeSec, float temperatureMax) {
    if (alarmIsActive || isRunning()) {
      systemText = alarmIsActive ? "Старт запрещён: активна авария" : "Старт запрещён: тест уже запущен";
      return false;
    }

    targetPowerW = powerSet;
    currentLimitA = currentMax;
    voltageLimitV = voltageMin;
    durationSec = timeSec;
    temperatureLimitC = temperatureMax;
    finishedElapsedSec = 0;
    startMs = millis();
    overCurrentConfirmCounter = 0;
    pwmCurrent = 0.0;
    resetRegulators();
    writeLoadPwm(0);
    workMode = CTRL_P_CONST;
    systemText = "Тест P = const запущен";

    return true;
  }

  void stop() {
    if (alarmIsActive) {
      systemText = "Остановка игнорирована: активна авария, требуется сброс";
      return;
    }

    finishedElapsedSec = getElapsedSec();
    stopOutput();
    overCurrentConfirmCounter = 0;
    workMode = CTRL_WAITING;
    systemText = "Тест остановлен";
  }

  void emergencyStop() {
    setAlarm("EMERGENCY_STOP");
  }

  void resetAlarm() {
    if (!alarmIsActive) {
      return;
    }

    alarmIsActive = false;
    systemText = "Авария сброшена";
    workMode = CTRL_WAITING;
    finishedElapsedSec = 0;
    overCurrentConfirmCounter = 0;
    stopOutput();
  }

  void setMessage(String text) {
    systemText = text;
  }

  bool canStart() {
    return !alarmIsActive && !isRunning();
  }

  void updateFan(float temperatureC) {
    if (isRunning()) {
      fanIsActive = true;
    }

    if (temperatureC >= FAN_ON_TEMP_C) {
      fanIsActive = true;
    }

    if (!isRunning() && temperatureC <= FAN_OFF_TEMP_C) {
      fanIsActive = false;
    }

    ledcWrite(FAN_PWM_PIN, fanIsActive ? 255 : 0);
  }

  bool isRunning() {
    return workMode == CTRL_I_CONST || workMode == CTRL_P_CONST;
  }

  bool hasAlarm() {
    return alarmIsActive;
  }

  bool isFanOn() {
    return fanIsActive;
  }

  int getPwm() {
    return pwmOutput;
  }

  unsigned long getElapsedSec() {
    if (isRunning()) {
      return (millis() - startMs) / 1000UL;
    }

    return finishedElapsedSec;
  }

  String getModeText() {
    if (workMode == CTRL_I_CONST) {
      return "I = const";
    }

    if (workMode == CTRL_P_CONST) {
      return "P = const";
    }

    if (workMode == CTRL_ERROR) {
      return "Авария";
    }

    return "Ожидание";
  }

  const char* getModeCode() {
    if (workMode == CTRL_I_CONST) {
      return "I_CONST";
    }

    if (workMode == CTRL_P_CONST) {
      return "P_CONST";
    }

    if (workMode == CTRL_ERROR) {
      return "ERROR";
    }

    return "WAITING";
  }

  String getMessage() {
    return systemText;
  }
};

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

  String encode(String value) {
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

public:
  TelemetryClient() :
    serverUrl(TELEMETRY_SERVER_URL),
    sendPeriodMs(TELEMETRY_PERIOD_MS),
    httpTimeoutMs(TELEMETRY_HTTP_TIMEOUT_MS),
    lastSendMs(0),
    lastSerialLogMs(0),
    pauseUntilMs(0),
    failCounter(0),
    enabled(ENABLE_HTTP_TELEMETRY) {
  }

  void begin() {
    lastSendMs = 0;
    lastSerialLogMs = 0;
    pauseUntilMs = 0;
    failCounter = 0;
  }

  bool shouldSend() {
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

  void send(String line) {
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

  String buildLine(String timestamp, String eventName, String infoText, Sensors &sensors, LoadController &load) {
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

  void sendPeriodic(Sensors &sensors, LoadController &load, String timestamp) {
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

  void setEnabled(bool value) {
    enabled = value;
  }

  void setServerUrl(const char *url) {
    serverUrl = url;
  }

  void setPeriodMs(unsigned long value) {
    sendPeriodMs = value;
  }
};

Sensors smartSensors;
LoadController smartLoad;
HttpInterface smartHttp;
TelemetryClient smartTelemetry;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting SmartLoad ESP32 version 1.4.2...");
  Serial.println("timestamp,event,current_A,voltage_V,power_W,temp_C,pwm,elapsed_s,mode,info");

  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  smartLoad.begin();
  smartSensors.begin();
  smartTelemetry.begin();

  delay(300);

  if (smartSensors.calibrateCurrentZero()) {
    Serial.print("ZERO OK. ZeroDiff raw = ");
    Serial.println(smartSensors.getCurrentZeroRaw());
  } else {
    smartLoad.setMessage("Первичная калибровка нуля не выполнена");
    Serial.println("ZERO FAILED ON STARTUP");
  }

  Serial.print("Voltage K = ");
  Serial.println(smartSensors.getVoltageDividerK(), 4);

  smartHttp.begin(server, smartSensors, smartLoad);

  Serial.println("System ready");
}

void loop() {
  smartHttp.update();

  if (millis() - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = millis();
    smartSensors.update();
  }

  if (millis() - lastControlMs >= CONTROL_PERIOD_MS) {
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

  smartTelemetry.sendPeriodic(smartSensors, smartLoad, smartHttp.getTimestamp());

  digitalWrite(LED_STATUS_PIN, smartLoad.isRunning() ? HIGH : LOW);

  delay(2);
}
