#pragma once

#include <Arduino.h>
#include <pgmspace.h>

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
        <button class="btn-main" onclick="openSettings()">Настроить</button>
      </div>

      <div class="mode-item">
        <div class="mode-top">
          <div class="mode-name">Стабилизация мощности</div>
          <div class="mode-code">P = const</div>
        </div>
        <button class="btn-main" onclick="openPowerSettings()">Настроить</button>
      </div>

      <div class="mode-item">
        <div class="mode-top">
          <div class="mode-name">PI-регулятор</div>
          <div class="mode-code">debug</div>
        </div>
        <button class="btn-neutral" onclick="location.href='/debug'">Коэффициенты и шаг PWM</button>
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
      <input id="currentInput" type="number" step="0.1" min="0.1" max="5.0" value="5.0">
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
      <input id="imaxInput" type="number" step="0.1" min="0.1" max="5.0" value="5.0">
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

let dataRequestBusy = false;
let dataRefreshTimer = null;

function scheduleDataUpdate(delayMs){
  if (dataRefreshTimer) {
    clearTimeout(dataRefreshTimer);
  }

  dataRefreshTimer = setTimeout(getData, delayMs);
}

function getData(){
  if (dataRequestBusy) {
    scheduleDataUpdate(1000);
    return;
  }

  dataRequestBusy = true;

  fetch('/data')
    .then(r => r.json())
    .then(d => updatePage(d))
    .catch(e => console.log(e))
    .finally(() => {
      dataRequestBusy = false;
      scheduleDataUpdate(1000);
    });
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

setInterval(syncClientTime, 10000);

window.onload = function(){
  syncClientTime();
  getData();
};
</script>
</body>
</html>
)rawliteral";

const char DEBUG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SMARTLOAD ESP32 DEBUG</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#11161d;color:#eef2f7;font-family:Arial,sans-serif;padding:14px}
.wrapper{width:100%;max-width:760px;margin:0 auto}
.card{background:#1a212b;border:1px solid #2d3746;border-radius:18px;padding:16px;margin-bottom:14px;box-shadow:0 8px 24px rgba(0,0,0,.25)}
.header{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.title{font-size:28px;font-weight:800;letter-spacing:.3px}
.back{color:#9db5d4;text-decoration:none;font-weight:800}
.section-title{font-size:22px;font-weight:800;margin-bottom:14px;color:#dfe8f7}
.field-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.field{background:#121820;border:1px solid #2b3443;border-radius:14px;padding:14px}
label{display:block;font-size:16px;font-weight:800;margin-bottom:6px;color:#fff}
.hint{min-height:38px;color:#9eacc0;font-size:14px;line-height:1.35;margin-bottom:10px}
input{width:100%;padding:13px;border-radius:12px;border:1px solid #354153;background:#0f151d;color:#fff;font-size:18px}
.diag-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.diag-item{background:#121820;border:1px solid #2b3443;border-radius:14px;padding:12px}
.diag-label{font-size:13px;color:#9eacc0;margin-bottom:5px}
.diag-value{font-size:20px;font-weight:800;color:#fff;word-break:break-word}
button{width:100%;border:none;border-radius:14px;padding:14px 12px;font-size:18px;font-weight:800;cursor:pointer;color:#fff}
button:active{transform:scale(.99)}
.btn-main{background:#188a47}
.btn-neutral{background:#596273}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.message{display:none;background:#121820;border:1px solid #334054;border-radius:14px;padding:12px;color:#dbe5f4;font-size:15px;line-height:1.35;margin-bottom:14px}
@media(max-width:640px){
  .title{font-size:24px}
  .field-grid{grid-template-columns:1fr}
  .diag-grid{grid-template-columns:1fr 1fr}
  .actions{grid-template-columns:1fr}
}
</style>
</head>
<body>
<div class="wrapper">
  <div class="card">
    <div class="header">
      <div class="title">Настройки PI-регулятора</div>
      <a class="back" href="/">Назад</a>
    </div>
  </div>

  <div id="messageBox" class="message"></div>

  <div class="card">
    <div class="section-title">I = const</div>
    <div class="field-grid">
      <div class="field">
        <label for="kpI">kpI</label>
        <div class="hint">P-часть тока: реагирует на изменение ошибки и помогает быстрее подойти к заданному току.</div>
        <input id="kpI" type="number" step="0.01" min="0">
      </div>
      <div class="field">
        <label for="kiI">kiI</label>
        <div class="hint">I-часть тока: постепенно дотягивает PWM, пока ток отличается от заданного.</div>
        <input id="kiI" type="number" step="0.01" min="0">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">P = const</div>
    <div class="field-grid">
      <div class="field">
        <label for="kpP">kpP</label>
        <div class="hint">P-часть мощности: реагирует на изменение ошибки по мощности.</div>
        <input id="kpP" type="number" step="0.001" min="0">
      </div>
      <div class="field">
        <label for="kiP">kiP</label>
        <div class="hint">I-часть мощности: плавно дотягивает нагрузку до заданной мощности.</div>
        <input id="kiP" type="number" step="0.001" min="0">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Ограничение изменения PWM</div>
    <div class="field-grid">
      <div class="field">
        <label for="stepUp">PWM_STEP_UP_MAX</label>
        <div class="hint">Максимальный шаг роста PWM за один цикл управления. Ограничивает резкое открытие MOSFET.</div>
        <input id="stepUp" type="number" step="0.1" min="0.01">
      </div>
      <div class="field">
        <label for="stepDown">PWM_STEP_DOWN_MAX</label>
        <div class="hint">Максимальный шаг снижения PWM за один цикл. Позволяет быстрее уменьшать ток при превышении.</div>
        <input id="stepDown" type="number" step="0.1" min="0.01">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">PWM / MOSFET / protections</div>
    <div class="field-grid">
      <div class="field">
        <label for="loadOutputEnabled">ENABLE_LOAD_OUTPUT</label>
        <div class="hint">1 = real PWM to MOSFET, 0 = force output to 0.</div>
        <input id="loadOutputEnabled" type="number" step="1" min="0" max="1">
      </div>
      <div class="field">
        <label for="voltageProtectionEnabled">ENABLE_VOLTAGE_PROTECTION</label>
        <div class="hint">1 = stop by Vmin, 0 = low-voltage protection off.</div>
        <input id="voltageProtectionEnabled" type="number" step="1" min="0" max="1">
      </div>
      <div class="field">
        <label for="pwmMin">PWM_MIN</label>
        <div class="hint">Minimum non-zero PWM. Stop still writes 0.</div>
        <input id="pwmMin" type="number" step="1" min="0">
      </div>
      <div class="field">
        <label for="pwmMax">PWM_MAX</label>
        <div class="hint">Maximum PWM. For 8 bit usually 255.</div>
        <input id="pwmMax" type="number" step="1" min="0">
      </div>
      <div class="field">
        <label for="pwmFreq">PWM_FREQ_HZ</label>
        <div class="hint">PWM frequency. Changing it resets load output.</div>
        <input id="pwmFreq" type="number" step="1" min="1" max="40000">
      </div>
      <div class="field">
        <label for="pwmResolution">PWM_RESOLUTION_BITS</label>
        <div class="hint">PWM resolution. Changing it resets load output.</div>
        <input id="pwmResolution" type="number" step="1" min="1" max="16">
      </div>
      <div class="field">
        <label for="maxTestCurrent">MAX_TEST_CURRENT_A</label>
        <div class="hint">Top limit for I target and Imax while debugging.</div>
        <input id="maxTestCurrent" type="number" step="0.1" min="0.1">
      </div>
      <div class="field">
        <label for="overCurrentFactor">OVER_CURRENT_FACTOR</label>
        <div class="hint">Overcurrent multiplier in I = const mode.</div>
        <input id="overCurrentFactor" type="number" step="0.01" min="1">
      </div>
      <div class="field">
        <label for="overCurrentConfirmCount">OVER_CURRENT_CONFIRM_COUNT</label>
        <div class="hint">Control cycles above limit before alarm.</div>
        <input id="overCurrentConfirmCount" type="number" step="1" min="1" max="100">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Вентилятор</div>
    <div class="field-grid">
      <div class="field">
        <label for="fanOnTemp">Температура включения, °C</label>
        <div class="hint">При этой температуре вентилятор включается на максимум. Выключение остаётся по FAN_OFF_TEMP_C из config.h.</div>
        <input id="fanOnTemp" type="number" step="1" min="0" max="120">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Измерение тока и напряжения</div>
    <div class="field-grid">
      <div class="field">
        <label for="currentK">currentK</label>
        <div class="hint">Коэффициент пересчёта разницы ADC в ток. Удобно менять при подборе шунта и усилителя.</div>
        <input id="currentK" type="number" step="0.00001" min="0.00001" max="10">
      </div>
      <div class="field">
        <label for="voltageK">voltageK</label>
        <div class="hint">Коэффициент делителя входного напряжения. Увеличить, если ESP32 показывает меньше реального.</div>
        <input id="voltageK" type="number" step="0.001" min="0.001" max="500">
      </div>
      <div class="field">
        <label for="adcRefVoltage">ADC_REF_VOLTAGE</label>
        <div class="hint">ADC reference voltage used for raw-to-volt conversion.</div>
        <input id="adcRefVoltage" type="number" step="0.001" min="0.1" max="5">
      </div>
      <div class="field">
        <label for="adcMaxValue">ADC_MAX_VALUE</label>
        <div class="hint">Maximum ADC raw code. For 12 bit usually 4095.</div>
        <input id="adcMaxValue" type="number" step="1" min="1" max="65535">
      </div>
      <div class="field">
        <label for="analogAverageSamples">ADC_AVERAGE_SAMPLES</label>
        <div class="hint">New P/N sample pairs read on each sensor update.</div>
        <input id="analogAverageSamples" type="number" step="1" min="1" max="512">
      </div>
      <div class="field">
        <label for="movingAverageSamples">ADC_MOVING_AVERAGE_SAMPLES</label>
        <div class="hint">Moving-average buffer length for raw P/N pairs.</div>
        <input id="movingAverageSamples" type="number" step="1" min="1" max="512">
      </div>
      <div class="field">
        <label for="filterK">FILTER_K</label>
        <div class="hint">Measurement filter factor. Higher = faster, noisier.</div>
        <input id="filterK" type="number" step="0.01" min="0.01" max="1">
      </div>
      <div class="field">
        <label for="currentZeroSamples">CURRENT_ZERO_SAMPLES</label>
        <div class="hint">Sample count for current auto-zero before start.</div>
        <input id="currentZeroSamples" type="number" step="1" min="1" max="500">
      </div>
      <div class="field">
        <label for="currentZeroStabilityRaw">CURRENT_ZERO_STABILITY_RAW</label>
        <div class="hint">Maximum raw spread allowed during auto-zero.</div>
        <input id="currentZeroStabilityRaw" type="number" step="1" min="1" max="4095">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Диагностика схемы</div>
    <div class="diag-grid">
      <div class="diag-item"><div class="diag-label">Ток</div><div id="dbgCurrent" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Напряжение</div><div id="dbgVoltage" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Мощность</div><div id="dbgPower" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Температура</div><div id="dbgTemp" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">PWM</div><div id="dbgPwm" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Режим</div><div id="dbgMode" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Current diff raw</div><div id="dbgCurrentDiff" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Current raw P</div><div id="dbgCurrentRawP" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Current raw N</div><div id="dbgCurrentRawN" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Zero raw</div><div id="dbgCurrentZero" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Voltage diff raw</div><div id="dbgVoltageDiff" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Voltage raw P</div><div id="dbgVoltageRawP" class="diag-value">--</div></div>
      <div class="diag-item"><div class="diag-label">Voltage raw N</div><div id="dbgVoltageRawN" class="diag-value">--</div></div>
    </div>
  </div>

  <div class="card">
    <div class="actions">
      <button class="btn-main" onclick="saveDebug()">Применить</button>
      <button class="btn-neutral" onclick="resetDebug()">Сбросить к config.h</button>
    </div>
  </div>
</div>

<script>
function setMessage(text){
  let box = document.getElementById('messageBox');
  box.innerText = text;
  box.style.display = text ? 'block' : 'none';
}

function setValues(data){
  document.getElementById('kpI').value = Number(data.kpI).toFixed(3);
  document.getElementById('kiI').value = Number(data.kiI).toFixed(3);
  document.getElementById('kpP').value = Number(data.kpP).toFixed(4);
  document.getElementById('kiP').value = Number(data.kiP).toFixed(4);
  document.getElementById('stepUp').value = Number(data.stepUp).toFixed(2);
  document.getElementById('stepDown').value = Number(data.stepDown).toFixed(2);
  document.getElementById('fanOnTemp').value = Number(data.fanOnTemp).toFixed(1);
  document.getElementById('maxTestCurrent').value = Number(data.maxTestCurrent).toFixed(1);
  document.getElementById('overCurrentFactor').value = Number(data.overCurrentFactor).toFixed(2);
  document.getElementById('overCurrentConfirmCount').value = Number(data.overCurrentConfirmCount);
  document.getElementById('loadOutputEnabled').value = data.loadOutputEnabled ? 1 : 0;
  document.getElementById('voltageProtectionEnabled').value = data.voltageProtectionEnabled ? 1 : 0;
  document.getElementById('pwmMin').value = Number(data.pwmMin);
  document.getElementById('pwmMax').value = Number(data.pwmMax);
  document.getElementById('pwmFreq').value = Number(data.pwmFreq);
  document.getElementById('pwmResolution').value = Number(data.pwmResolution);
  document.getElementById('currentK').value = Number(data.currentK).toFixed(5);
  document.getElementById('voltageK').value = Number(data.voltageK).toFixed(3);
  document.getElementById('adcRefVoltage').value = Number(data.adcRefVoltage).toFixed(3);
  document.getElementById('adcMaxValue').value = Number(data.adcMaxValue).toFixed(0);
  document.getElementById('analogAverageSamples').value = Number(data.analogAverageSamples);
  document.getElementById('movingAverageSamples').value = Number(data.movingAverageSamples);
  document.getElementById('filterK').value = Number(data.filterK).toFixed(2);
  document.getElementById('currentZeroSamples').value = Number(data.currentZeroSamples);
  document.getElementById('currentZeroStabilityRaw').value = Number(data.currentZeroStabilityRaw).toFixed(1);
  updateDiagnostics(data);
}

function setDiag(id, value){
  document.getElementById(id).innerText = value;
}

function updateDiagnostics(data){
  setDiag('dbgCurrent', Number(data.debugCurrent).toFixed(3) + ' А');
  setDiag('dbgVoltage', Number(data.debugVoltage).toFixed(2) + ' В');
  setDiag('dbgPower', Number(data.debugPower).toFixed(1) + ' Вт');
  setDiag('dbgTemp', Number(data.debugTemp).toFixed(1) + ' °C');
  setDiag('dbgPwm', String(data.debugPwm));
  setDiag('dbgMode', String(data.debugMode));
  setDiag('dbgCurrentDiff', Number(data.currentDiffRaw).toFixed(1));
  setDiag('dbgCurrentRawP', Number(data.currentRawP).toFixed(1));
  setDiag('dbgCurrentRawN', Number(data.currentRawN).toFixed(1));
  setDiag('dbgCurrentZero', Number(data.currentZeroRaw).toFixed(1));
  setDiag('dbgVoltageDiff', Number(data.voltageDiffRaw).toFixed(1));
  setDiag('dbgVoltageRawP', Number(data.voltageRawP).toFixed(1));
  setDiag('dbgVoltageRawN', Number(data.voltageRawN).toFixed(1));
}

function loadDebug(){
  fetch('/debugdata')
    .then(r => r.json())
    .then(d => setValues(d))
    .catch(e => setMessage('Не удалось загрузить настройки'));
}

function refreshDiagnostics(){
  fetch('/debugdata')
    .then(r => r.json())
    .then(d => updateDiagnostics(d))
    .catch(e => console.log(e));
}

function valueParam(id){
  return encodeURIComponent(document.getElementById(id).value);
}

function saveDebug(){
  let url = '/debugsave?kpI=' + valueParam('kpI') +
            '&kiI=' + valueParam('kiI') +
            '&kpP=' + valueParam('kpP') +
            '&kiP=' + valueParam('kiP') +
            '&stepUp=' + valueParam('stepUp') +
            '&stepDown=' + valueParam('stepDown') +
            '&fanOnTemp=' + valueParam('fanOnTemp') +
            '&maxTestCurrent=' + valueParam('maxTestCurrent') +
            '&overCurrentFactor=' + valueParam('overCurrentFactor') +
            '&overCurrentConfirmCount=' + valueParam('overCurrentConfirmCount') +
            '&loadOutputEnabled=' + valueParam('loadOutputEnabled') +
            '&voltageProtectionEnabled=' + valueParam('voltageProtectionEnabled') +
            '&pwmMin=' + valueParam('pwmMin') +
            '&pwmMax=' + valueParam('pwmMax') +
            '&pwmFreq=' + valueParam('pwmFreq') +
            '&pwmResolution=' + valueParam('pwmResolution') +
            '&currentK=' + valueParam('currentK') +
            '&voltageK=' + valueParam('voltageK') +
            '&adcRefVoltage=' + valueParam('adcRefVoltage') +
            '&adcMaxValue=' + valueParam('adcMaxValue') +
            '&analogAverageSamples=' + valueParam('analogAverageSamples') +
            '&movingAverageSamples=' + valueParam('movingAverageSamples') +
            '&filterK=' + valueParam('filterK') +
            '&currentZeroSamples=' + valueParam('currentZeroSamples') +
            '&currentZeroStabilityRaw=' + valueParam('currentZeroStabilityRaw');

  fetch(url)
    .then(r => r.json())
    .then(d => {
      setValues(d);
      setMessage('Настройки применены. После перезагрузки вернутся значения из config.h.');
    })
    .catch(e => setMessage('Не удалось применить настройки'));
}

function resetDebug(){
  fetch('/debugreset')
    .then(r => r.json())
    .then(d => {
      setValues(d);
      setMessage('Настройки сброшены к значениям из config.h.');
    })
    .catch(e => setMessage('Не удалось сбросить настройки'));
}

window.onload = function(){
  loadDebug();
  setInterval(refreshDiagnostics, 1000);
};
</script>
</body>
</html>
)rawliteral";
