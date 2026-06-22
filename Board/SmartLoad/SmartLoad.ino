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

// =====================================================
// Пины по актуальной схеме SmartLoad.
// =====================================================

#define LED_STATUS_PIN      2

#define ADC_CURRENT_N_PIN   34    // CurrentSensorN
#define ADC_CURRENT_P_PIN   35    // CurrentSensorP
#define ADC_VOLTAGE_PIN     32    // VoltageSensor

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

// Коэффициент пересчёта тока. Потом уточнить по внешнему амперметру.
float currentK = 0.007326;

// PI-регулятор тока для режима I = const.
// Мягкие стартовые коэффициенты для первого запуска.
float kpI = 0.8;       // P-часть: реакция на изменение ошибки
float kiI = 1.5;       // I-часть: постепенное дотягивание тока
float lastErrorI = 0.0;

// PI-регулятор мощности для режима P = const.
float kpP = 0.03;
float kiP = 0.08;
float lastErrorP = 0.0;

// Если CurrentSensorP и CurrentSensorN фактически перепутаны, поставить 1.
#define CURRENT_DIFF_INVERTED       0

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
// Режимы работы
// =====================================================

enum WorkMode {
  MODE_WAITING,
  MODE_I_CONST,
  MODE_P_CONST,
  MODE_ERROR
};

WorkMode mode = MODE_WAITING;

// =====================================================
// Состояние системы
// =====================================================

bool alarmState = false;
bool fanState = false;
bool filterReady = false;

float setCurrentA = 10.0;
float setPowerW = 100.0;
float maxCurrentA = 10.0;
float minVoltageV = 42.0;
float maxTemperatureC = 70.0;
int testTimeSec = 300;

float measuredCurrentA = 0.0;
float measuredVoltageV = 0.0;
float measuredPowerW = 0.0;
float radiatorTempC = -125.0;

float instantCurrentA = 0.0;
float instantVoltageV = 0.0;

float filteredCurrentA = 0.0;
float filteredVoltageV = 0.0;

float currentZeroDiffRaw = 0.0;

int pwmValue = 0;
float pwmFloat = 0.0;

int overCurrentCounter = 0;

unsigned long testStartMs = 0;
unsigned long savedElapsedSec = 0;

unsigned long lastSensorMs = 0;
unsigned long lastControlMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastTelemetryMs = 0;

String systemMessage = "";

// =====================================================
// Время от клиента
// =====================================================

bool clientTimeValid = false;
int64_t clientEpochMs = 0;
int clientTzOffsetMin = 0;
unsigned long clientTimeSyncMs = 0;

// =====================================================
// DS18B20
// =====================================================

OneWire ds(TEMP_ONEWIRE_PIN);

bool tempRequestStarted = false;
unsigned long tempRequestMs = 0;

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

// =====================================================
// Вспомогательные функции
// =====================================================

// Преобразует строку в 64-битное целое число.
int64_t parseInt64(const String &s) {
  int64_t value = 0;
  bool negative = false;
  int start = 0;

  if (s.length() > 0 && s[0] == '-') {
    negative = true;
    start = 1;
  }

  for (int i = start; i < s.length(); i++) {
    char c = s[i];

    if (c < '0' || c > '9') {
      break;
    }

    value = value * 10 + (c - '0');
  }

  return negative ? -value : value;
}

// Экранирует строку для JSON-ответа.
String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", " ");
  return s;
}

// Кодирует строку для отправки в application/x-www-form-urlencoded.
String urlEncode(String value) {
  const char hex[] = "0123456789ABCDEF";
  String encoded = "";

  for (int i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];

    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
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

// Обновляет время ESP32 по параметрам, полученным от браузера.
void updateClientTimeFromArgs() {
  if (!server.hasArg("epoch")) {
    return;
  }

  clientEpochMs = parseInt64(server.arg("epoch"));

  if (server.hasArg("tz")) {
    clientTzOffsetMin = server.arg("tz").toInt();
  }

  clientTimeSyncMs = millis();
  clientTimeValid = true;
}

// Возвращает текущее время для лога.
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

// Возвращает короткий код текущего режима.
const char* getModeCode() {
  switch (mode) {
    case MODE_WAITING:
      return "WAITING";
    case MODE_I_CONST:
      return "I_CONST";
    case MODE_P_CONST:
      return "P_CONST";
    case MODE_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

// Возвращает текст текущего режима для веб-интерфейса.
const char* getModeText() {
  switch (mode) {
    case MODE_WAITING:
      return "Ожидание";
    case MODE_I_CONST:
      return "I = const";
    case MODE_P_CONST:
      return "P = const";
    case MODE_ERROR:
      return "Авария";
    default:
      return "Ошибка";
  }
}

bool isWorkMode() {
  return mode == MODE_I_CONST || mode == MODE_P_CONST;
}

// Возвращает коэффициент делителя входного напряжения.
float getVoltageK() {
  return (VOLTAGE_R_TOP + VOLTAGE_R_BOTTOM) / VOLTAGE_R_BOTTOM;
}

// Возвращает прошедшее время текущего или последнего теста.
unsigned long getElapsedSec() {
  if (isWorkMode() && testStartMs > 0) {
    return (millis() - testStartMs) / 1000;
  }

  return savedElapsedSec;
}

// =====================================================
// PWM
// =====================================================

// Инициализирует PWM нагрузки и вентиляторов.
void initPwm() {
  ledcAttach(LOAD_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
  ledcAttach(FAN_PWM_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);

  ledcWrite(LOAD_PWM_PIN, 0);
  ledcWrite(FAN_PWM_PIN, 0);
}

// Выводит PWM на силовую часть с учётом инверсии и безопасного режима.
void writeLoadPwmHardware(int value) {
  int out = value;

#if PWM_INVERTED
  out = PWM_MAX - value;
#endif

#if ENABLE_LOAD_OUTPUT
  ledcWrite(LOAD_PWM_PIN, out);
#else
  ledcWrite(LOAD_PWM_PIN, 0);
#endif
}

// Ограничивает и сохраняет PWM нагрузки.
void setLoadPwm(int value) {
  if (value < PWM_MIN) {
    value = PWM_MIN;
  }

  if (value > PWM_MAX) {
    value = PWM_MAX;
  }

  pwmValue = value;
  writeLoadPwmHardware(pwmValue);
}

// Полностью отключает нагрузку и сбрасывает состояние регулятора.
void disableLoad() {
  pwmFloat = 0.0;
  lastErrorI = 0.0;
  lastErrorP = 0.0;
  setLoadPwm(0);
}

// Ограничивает и задаёт PWM вентиляторов.
void setFanPwm(int value) {
  if (value < 0) {
    value = 0;
  }

  if (value > 255) {
    value = 255;
  }

  ledcWrite(FAN_PWM_PIN, value);
}

// =====================================================
// Датчики
// =====================================================

// Читает аналоговый вход с усреднением.
float readAnalogAverage(uint8_t pin) {
  long sum = 0;

  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }

  return sum / 16.0;
}

// Обновляет температуру DS18B20 без долгой блокировки loop().
void updateTemperatureSensor() {
  if (!tempRequestStarted) {
    if (ds.reset()) {
      ds.write(0xCC);
      ds.write(0x44);
      tempRequestStarted = true;
      tempRequestMs = millis();
    }

    return;
  }

  if (millis() - tempRequestMs < 800) {
    return;
  }

  byte dataTemp[9];

  if (ds.reset()) {
    ds.write(0xCC);
    ds.write(0xBE);

    for (int i = 0; i < 9; i++) {
      dataTemp[i] = ds.read();
    }

    if (OneWire::crc8(dataTemp, 8) == dataTemp[8]) {
      int16_t rawTemp = (dataTemp[1] << 8) | dataTemp[0];
      float temp = rawTemp * 0.0625;

      if (temp > -55.0 && temp < 125.0) {
        radiatorTempC = temp;
      }
    }
  }

  tempRequestStarted = false;
}

// Читает ток, напряжение, мощность и температуру.
void readSensors() {
  float rawP = readAnalogAverage(ADC_CURRENT_P_PIN);
  float rawN = readAnalogAverage(ADC_CURRENT_N_PIN);

  float currentDiffRaw = rawP - rawN;

#if CURRENT_DIFF_INVERTED
  currentDiffRaw = -currentDiffRaw;
#endif

  instantCurrentA = (currentDiffRaw - currentZeroDiffRaw) * currentK;

  if (instantCurrentA < 0.0) {
    instantCurrentA = 0.0;
  }

  if (instantCurrentA < CURRENT_DEAD_ZONE_A) {
    instantCurrentA = 0.0;
  }

  float rawVoltage = readAnalogAverage(ADC_VOLTAGE_PIN);

  float adcVoltage = rawVoltage * ADC_REF_VOLTAGE / ADC_MAX_VALUE;
  instantVoltageV = adcVoltage * getVoltageK();

  if (instantVoltageV < VOLTAGE_DEAD_ZONE_V) {
    instantVoltageV = 0.0;
  }

  if (!filterReady) {
    filteredCurrentA = instantCurrentA;
    filteredVoltageV = instantVoltageV;
    filterReady = true;
  } else {
    filteredCurrentA = filteredCurrentA + FILTER_K * (instantCurrentA - filteredCurrentA);
    filteredVoltageV = filteredVoltageV + FILTER_K * (instantVoltageV - filteredVoltageV);
  }

  measuredCurrentA = filteredCurrentA;
  measuredVoltageV = filteredVoltageV;
  measuredPowerW = measuredVoltageV * measuredCurrentA;

  updateTemperatureSensor();
}

// =====================================================
// Автокалибровка нуля тока
// =====================================================

// Измеряет ноль дифференциального датчика тока перед запуском.
bool calibrateCurrentZero() {
  if (isWorkMode()) {
    systemMessage = "Калибровка нуля запрещена во время теста";
    Serial.println("ZERO BLOCKED: TEST IS RUNNING");
    return false;
  }

  disableLoad();
  delay(300);

  float sum = 0.0;
  float minDiff = 1000000.0;
  float maxDiff = -1000000.0;

  for (int i = 0; i < CURRENT_ZERO_SAMPLES; i++) {
    float rawP = readAnalogAverage(ADC_CURRENT_P_PIN);
    float rawN = readAnalogAverage(ADC_CURRENT_N_PIN);

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
    systemMessage = "Калибровка нуля не выполнена: сигнал тока нестабилен";
    Serial.print("ZERO FAILED. Spread raw = ");
    Serial.println(spread);
    return false;
  }

  currentZeroDiffRaw = sum / CURRENT_ZERO_SAMPLES;

  instantCurrentA = 0.0;
  measuredCurrentA = 0.0;
  filteredCurrentA = 0.0;
  filterReady = false;

  systemMessage = "Калибровка нуля выполнена";

  Serial.print("ZERO OK. ZeroDiff raw = ");
  Serial.println(currentZeroDiffRaw);

  return true;
}

// =====================================================
// Логирование и телеметрия
// =====================================================

// Отправляет строку лога на Python HTTP-сервер.
void sendLogToServer(String line) {
#if ENABLE_HTTP_TELEMETRY
  if (WiFi.softAPgetStationNum() == 0) {
    return;
  }

  HTTPClient http;
  http.begin(TELEMETRY_SERVER_URL);
  http.setTimeout(TELEMETRY_HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST("line=" + urlEncode(line));
  http.end();
#else
  (void)line;
#endif
}

// Формирует строку телеметрии в формате CSV.
String buildLogLine(String eventName, String infoText) {
  String line = "";

  line += getTimestamp();
  line += ",";
  line += eventName;
  line += ",";
  line += String(measuredCurrentA, 3);
  line += ",";
  line += String(measuredVoltageV, 2);
  line += ",";
  line += String(measuredPowerW, 1);
  line += ",";
  line += String(radiatorTempC, 1);
  line += ",";
  line += String(pwmValue);
  line += ",";
  line += String(getElapsedSec());
  line += ",";
  line += getModeCode();
  line += ",";
  line += infoText;

  return line;
}

// Формирует строку лога, пишет её в Serial и передаёт на сервер.
void writeLog(String eventName, String infoText) {
  String line = buildLogLine(eventName, infoText);

  Serial.println(line);
  sendLogToServer(line);
}

// Передаёт текущую телеметрию на сервер 10 раз в секунду, а в Serial пишет раз в секунду.
void writePeriodicTelemetry() {
  String eventName;

  if (isWorkMode()) {
    eventName = "DATA";
  } else if (mode == MODE_ERROR || alarmState) {
    eventName = "ERROR_STATE";
  } else {
    eventName = "IDLE";
  }

  if (millis() - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
    lastTelemetryMs = millis();
    sendLogToServer(buildLogLine(eventName, ""));
  }

  if (millis() - lastLogMs < LOG_PERIOD_MS) {
    return;
  }

  lastLogMs = millis();
  Serial.println(buildLogLine(eventName, ""));
}

// =====================================================
// Вентиляторы и регулирование
// =====================================================

// Управляет вентиляторами по режиму и температуре радиатора.
void controlFan() {
  if (isWorkMode()) {
    fanState = true;
  }

  if (radiatorTempC >= FAN_ON_TEMP_C) {
    fanState = true;
  }

  if (!isWorkMode() && radiatorTempC <= FAN_OFF_TEMP_C) {
    fanState = false;
  }

  setFanPwm(fanState ? 255 : 0);
}

// Выполняет один шаг PI-регулятора тока для режима I = const.
void regulateIConst() {
  float dt = CONTROL_PERIOD_MS / 1000.0;

  float error = setCurrentA - measuredCurrentA;

  float deltaP = kpI * (error - lastErrorI);
  float deltaI = kiI * error * dt;

  float delta = deltaP + deltaI;

  lastErrorI = error;

  if (delta > PWM_STEP_UP_MAX) {
    delta = PWM_STEP_UP_MAX;
  }

  if (delta < -PWM_STEP_DOWN_MAX) {
    delta = -PWM_STEP_DOWN_MAX;
  }

  pwmFloat += delta;

  if (pwmFloat < PWM_MIN) {
    pwmFloat = PWM_MIN;
  }

  if (pwmFloat > PWM_MAX) {
    pwmFloat = PWM_MAX;
  }

  setLoadPwm((int)pwmFloat);
}

// Выполняет один шаг PI-регулятора мощности для режима P = const.
void regulatePConst() {
  float dt = CONTROL_PERIOD_MS / 1000.0;

  float error = setPowerW - measuredPowerW;

  float deltaP = kpP * (error - lastErrorP);
  float deltaI = kiP * error * dt;

  float delta = deltaP + deltaI;

  lastErrorP = error;

  if (delta > PWM_STEP_UP_MAX) {
    delta = PWM_STEP_UP_MAX;
  }

  if (delta < -PWM_STEP_DOWN_MAX) {
    delta = -PWM_STEP_DOWN_MAX;
  }

  pwmFloat += delta;

  if (pwmFloat < PWM_MIN) {
    pwmFloat = PWM_MIN;
  }

  if (pwmFloat > PWM_MAX) {
    pwmFloat = PWM_MAX;
  }

  setLoadPwm((int)pwmFloat);
}

// =====================================================
// Старт, остановка и защиты
// =====================================================

// Запускает режим стабилизации тока.
void startIConst(float currentSet, float vminSet, int timeSet, float tempSet) {
  if (alarmState) {
    systemMessage = "Старт запрещён: активна авария";
    Serial.println("START BLOCKED: ALARM ACTIVE");
    return;
  }

  if (isWorkMode()) {
    systemMessage = "Старт запрещён: тест уже запущен";
    Serial.println("START BLOCKED: TEST IS RUNNING");
    return;
  }

  systemMessage = "Автокалибровка нуля тока...";

  if (!calibrateCurrentZero()) {
    systemMessage = "Старт запрещён: автокалибровка нуля не выполнена";
    Serial.println("START BLOCKED: AUTO ZERO FAILED");
    return;
  }

  readSensors();

  setCurrentA = currentSet;
  minVoltageV = vminSet;
  testTimeSec = timeSet;
  maxTemperatureC = tempSet;

  savedElapsedSec = 0;
  testStartMs = millis();
  lastControlMs = millis();
  lastLogMs = millis();
  lastTelemetryMs = millis();
  overCurrentCounter = 0;

  pwmFloat = 0.0;
  lastErrorI = 0.0;
  lastErrorP = 0.0;
  setLoadPwm(0);

  mode = MODE_I_CONST;
  systemMessage = "Тест I = const запущен";

  controlFan();

  writeLog("START", "Iset=" + String(setCurrentA, 2) +
                    " Vmin=" + String(minVoltageV, 2) +
                    " Time=" + String(testTimeSec) +
                    " Tmax=" + String(maxTemperatureC, 1));

  Serial.println("I const started");
}

// Запускает режим стабилизации мощности.
void startPConst(float powerSet, float imaxSet, float vminSet, int timeSet, float tempSet) {
  if (alarmState) {
    systemMessage = "Старт запрещён: активна авария";
    Serial.println("START BLOCKED: ALARM ACTIVE");
    return;
  }

  if (isWorkMode()) {
    systemMessage = "Старт запрещён: тест уже запущен";
    Serial.println("START BLOCKED: TEST IS RUNNING");
    return;
  }

  systemMessage = "Автокалибровка нуля тока...";

  if (!calibrateCurrentZero()) {
    systemMessage = "Старт запрещён: автокалибровка нуля не выполнена";
    Serial.println("START BLOCKED: AUTO ZERO FAILED");
    return;
  }

  readSensors();

  setPowerW = powerSet;
  maxCurrentA = imaxSet;
  minVoltageV = vminSet;
  testTimeSec = timeSet;
  maxTemperatureC = tempSet;

  savedElapsedSec = 0;
  testStartMs = millis();
  lastControlMs = millis();
  lastLogMs = millis();
  lastTelemetryMs = millis();
  overCurrentCounter = 0;

  pwmFloat = 0.0;
  lastErrorI = 0.0;
  lastErrorP = 0.0;
  setLoadPwm(0);

  mode = MODE_P_CONST;
  systemMessage = "Тест P = const запущен";

  controlFan();

  writeLog("START", "Pset=" + String(setPowerW, 1) +
                    " Imax=" + String(maxCurrentA, 2) +
                    " Vmin=" + String(minVoltageV, 2) +
                    " Time=" + String(testTimeSec) +
                    " Tmax=" + String(maxTemperatureC, 1));

  Serial.println("P const started");
}

// Выполняет штатную остановку теста.
void finishTest() {
  if (alarmState) {
    systemMessage = "Остановка игнорирована: активна авария, требуется сброс";
    Serial.println("FINISH IGNORED: ALARM ACTIVE");
    return;
  }

  savedElapsedSec = getElapsedSec();

  if (isWorkMode()) {
    writeLog("END", "");
  }

  mode = MODE_WAITING;
  overCurrentCounter = 0;

  disableLoad();
  controlFan();

  systemMessage = "Тест остановлен";
  Serial.println("Load ended");
}

// Завершает тест по заданному времени.
void finishByTime() {
  savedElapsedSec = getElapsedSec();

  if (isWorkMode()) {
    writeLog("END", "TIME_END");
  }

  mode = MODE_WAITING;
  overCurrentCounter = 0;

  disableLoad();
  controlFan();

  systemMessage = "Тест завершён по времени";
  Serial.println("Load ended by time");
}

// Переводит систему в аварийное состояние.
void setError(String errorText) {
  savedElapsedSec = getElapsedSec();

  if (isWorkMode()) {
    writeLog("ERROR", errorText);
  }

  alarmState = true;
  mode = MODE_ERROR;
  overCurrentCounter = 0;

  disableLoad();
  controlFan();

  systemMessage = "Авария: " + errorText;

  Serial.print("ERROR: ");
  Serial.println(errorText);
}

// Выполняет аварийный стоп.
void emergencyStop() {
  setError("EMERGENCY");
}

// Сбрасывает аварийную блокировку.
void resetAlarm() {
  if (!alarmState) {
    systemMessage = "Нет активной аварии";
    Serial.println("RESET IGNORED: NO ALARM");
    return;
  }

  alarmState = false;
  mode = MODE_WAITING;
  savedElapsedSec = 0;
  overCurrentCounter = 0;

  disableLoad();
  controlFan();

  systemMessage = "Авария сброшена";

  writeLog("RESET", "Alarm reset");

  Serial.println("Alarm reset");
}

// Проверяет программные защиты.
void checkProtection() {
  if (alarmState || !isWorkMode()) {
    return;
  }

  if (getElapsedSec() >= (unsigned long)testTimeSec) {
    finishByTime();
    return;
  }

#if ENABLE_VOLTAGE_PROTECTION
  if (minVoltageV > 0.0 && measuredVoltageV < minVoltageV) {
    setError("LOW_VOLTAGE");
    return;
  }
#endif

  if (radiatorTempC > maxTemperatureC) {
    setError("OVER_TEMPERATURE");
    return;
  }

  bool overCurrent = false;

  if (mode == MODE_I_CONST) {
    overCurrent = instantCurrentA > setCurrentA * OVER_CURRENT_FACTOR && instantCurrentA > 1.0;
  }

  if (mode == MODE_P_CONST) {
    overCurrent = maxCurrentA > 0.0 && instantCurrentA > maxCurrentA;
  }

  if (overCurrent) {
    overCurrentCounter++;

    if (overCurrentCounter >= OVER_CURRENT_CONFIRM_COUNT) {
      setError("OVER_CURRENT");
      return;
    }
  } else {
    overCurrentCounter = 0;
  }
}

// Обслуживает стенд в рабочем цикле.
void processStand() {
  if (!isWorkMode() || alarmState) {
    controlFan();
    return;
  }

  checkProtection();

  if (!isWorkMode() || alarmState) {
    controlFan();
    return;
  }

  if (millis() - lastControlMs >= CONTROL_PERIOD_MS) {
    lastControlMs = millis();

    if (mode == MODE_I_CONST) {
      regulateIConst();
    } else if (mode == MODE_P_CONST) {
      regulatePConst();
    }
  }

  controlFan();
}

// =====================================================
// HTTP-обработчики ESP32
// =====================================================

// Отправляет основные данные для веб-интерфейса.
void sendDataJson() {
  String json = "{";

  json += "\"mode\":\"";
  json += getModeCode();
  json += "\",";

  json += "\"modeText\":\"";
  json += getModeText();
  json += "\",";

  json += "\"alarm\":";
  json += (alarmState ? "true" : "false");
  json += ",";

  json += "\"current\":";
  json += String(measuredCurrentA, 3);
  json += ",";

  json += "\"voltage\":";
  json += String(measuredVoltageV, 2);
  json += ",";

  json += "\"power\":";
  json += String(measuredPowerW, 1);
  json += ",";

  json += "\"temp\":";
  json += String(radiatorTempC, 1);
  json += ",";

  json += "\"message\":\"";
  json += jsonEscape(systemMessage);
  json += "\"";

  json += "}";

  server.send(200, "application/json; charset=UTF-8", json);
}

// Отдаёт главную HTML-страницу.
void handleRoot() {
  server.send_P(200, "text/html; charset=UTF-8", INDEX_HTML);
}

// Отдаёт JSON с текущими измерениями.
void handleData() {
  sendDataJson();
}

// Принимает текущее время от браузера.
void handleClientTime() {
  updateClientTimeFromArgs();
  server.send(200, "application/json; charset=UTF-8", "{\"ok\":true}");
}

// Обрабатывает запуск режима I = const.
void handleStart() {
  updateClientTimeFromArgs();

  float currentSet = server.arg("i").toFloat();
  float vminSet = server.arg("vmin").toFloat();
  int timeSet = server.arg("time").toInt();
  float tempSet = server.arg("temp").toFloat();

  if (currentSet <= 0.0) {
    currentSet = 1.0;
  }

  if (vminSet < 0.0) {
    vminSet = 0.0;
  }

  if (timeSet <= 0) {
    timeSet = 300;
  }

  if (timeSet > 86400) {
    timeSet = 86400;
  }

  if (tempSet <= 0.0) {
    tempSet = 70.0;
  }

  startIConst(currentSet, vminSet, timeSet, tempSet);
  sendDataJson();
}

// Обрабатывает запуск режима P = const.
void handleStartP() {
  updateClientTimeFromArgs();

  float powerSet = server.arg("p").toFloat();
  float imaxSet = server.arg("imax").toFloat();
  float vminSet = server.arg("vmin").toFloat();
  int timeSet = server.arg("time").toInt();
  float tempSet = server.arg("temp").toFloat();

  if (powerSet <= 0.0) {
    powerSet = 10.0;
  }

  if (imaxSet <= 0.0) {
    imaxSet = 1.0;
  }

  if (vminSet < 0.0) {
    vminSet = 0.0;
  }

  if (timeSet <= 0) {
    timeSet = 300;
  }

  if (timeSet > 86400) {
    timeSet = 86400;
  }

  if (tempSet <= 0.0) {
    tempSet = 70.0;
  }

  startPConst(powerSet, imaxSet, vminSet, timeSet, tempSet);
  sendDataJson();
}

// Обрабатывает команды остановки, аварии и сброса.
void handleCommand() {
  updateClientTimeFromArgs();

  String action = server.arg("act");

  if (action == "off") {
    finishTest();
  }

  else if (action == "estop") {
    emergencyStop();
  }

  else if (action == "reset") {
    resetAlarm();
  }

  sendDataJson();
}

// Отвечает на неизвестные HTTP-адреса.
void handleNotFound() {
  server.send(404, "text/plain; charset=UTF-8", "Not found");
}

// =====================================================
// Инициализация
// =====================================================

// Настраивает пины и безопасное состояние выходов.
void initPins() {
  pinMode(LED_STATUS_PIN, OUTPUT);
  digitalWrite(LED_STATUS_PIN, LOW);

  initPwm();

  disableLoad();
  setFanPwm(0);
}

// Настраивает ADC и выполняет первичную калибровку нуля.
void initAdc() {
  analogReadResolution(12);

  analogSetPinAttenuation(ADC_CURRENT_P_PIN, ADC_11db);
  analogSetPinAttenuation(ADC_CURRENT_N_PIN, ADC_11db);
  analogSetPinAttenuation(ADC_VOLTAGE_PIN, ADC_11db);

  delay(300);

  calibrateCurrentZero();

  Serial.print("Voltage K = ");
  Serial.println(getVoltageK(), 4);
}

// Поднимает точку доступа и HTTP-сервер ESP32.
void initWifiAndServer() {
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Wi-Fi name: ");
  Serial.println(WIFI_SSID);

  Serial.print("Wi-Fi password: ");
  Serial.println(WIFI_PASSWORD);

  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/clienttime", handleClientTime);
  server.on("/start", handleStart);
  server.on("/startp", handleStartP);
  server.on("/cmd", handleCommand);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("Web server started");
}

// =====================================================
// Setup / loop
// =====================================================

// Точка входа Arduino: старт системы.
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting SmartLoad ESP32 version 1.4.2...");
  Serial.println("timestamp,event,current_A,voltage_V,power_W,temp_C,pwm,elapsed_s,mode,info");

  initPins();
  initAdc();
  initWifiAndServer();

  Serial.println("System ready");
}

// Главный цикл Arduino.
void loop() {
  server.handleClient();

  if (millis() - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = millis();
    readSensors();
  }

  processStand();
  writePeriodicTelemetry();

  digitalWrite(LED_STATUS_PIN, isWorkMode() ? HIGH : LOW);

  delay(2);
}
