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
body{background:#11161d;color:#eef2f7;font-family:Arial,sans-serif;padding:12px}
.wrapper{width:100%;max-width:920px;margin:0 auto}
.card{background:#1a212b;border:1px solid #2d3746;border-radius:8px;padding:12px;margin-bottom:10px}
.header{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.title{font-size:26px;font-weight:800;letter-spacing:.4px}
.status{padding:7px 12px;border-radius:999px;font-size:14px;font-weight:700}
.status-waiting{background:#2e3847;color:#d7e1ef}
.status-run{background:#163b24;color:#65ef98}
.status-error{background:#4b1717;color:#ff7c7c}
.message{display:none!important;margin-top:10px;background:#121820;border:1px solid #334054;border-radius:8px;padding:10px;color:#dbe5f4;font-size:14px;line-height:1.35}
.section-title{font-size:18px;font-weight:800;margin-bottom:10px;color:#dfe8f7}
.measure-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.measure-item{background:#121820;border:1px solid #2b3443;border-radius:6px;padding:10px}
.measure-label{font-size:13px;color:#98a6b8;margin-bottom:6px}
.measure-value{font-size:23px;font-weight:800;color:#fff}
.small-value{font-size:20px}
.mode-list{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.mode-item{background:#121820;border:1px solid #2b3443;border-radius:6px;padding:10px}
.mode-top{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:8px}
.mode-name{font-size:17px;font-weight:800}
.mode-code{font-size:14px;color:#9eacc0}
button{width:100%;border:none;border-radius:6px;padding:11px 10px;font-size:16px;font-weight:800;cursor:pointer;color:#fff}
button:active{transform:scale(.99)}
button:disabled{cursor:not-allowed;transform:none;opacity:.62}
.btn-main{background:#188a47}
.btn-neutral{background:#596273}
.btn-danger{background:#c22b2b}
.btn-reset{background:#2a7c37}
.action-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.footer-note{text-align:center;color:#3a4452;font-size:13px;margin-top:10px;user-select:none}
.modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,.72);align-items:center;justify-content:center;padding:16px;z-index:20}
.modal{width:100%;max-width:460px;background:#1a212b;border:1px solid #334054;border-radius:8px;padding:14px}
.modal-title{font-size:21px;font-weight:800;margin-bottom:14px}
.form-row{margin-bottom:10px}
.form-row label{display:block;margin-bottom:6px;color:#9aa8bb;font-size:14px}
input{width:100%;padding:11px;border-radius:6px;border:1px solid #354153;background:#121820;color:#fff;font-size:16px}
.modal-actions{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}
.confirm-text{color:#dbe5f4;line-height:1.5;margin-bottom:16px;font-size:16px}
@media(max-width:860px){
  .measure-grid{grid-template-columns:1fr 1fr}
  .mode-list{grid-template-columns:1fr}
  .action-grid{grid-template-columns:1fr}
}
@media(max-width:640px){
  .title{font-size:24px}
  .measure-grid{grid-template-columns:1fr}
  .modal-actions{grid-template-columns:1fr}
  .measure-value{font-size:22px}
  .small-value{font-size:19px}
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
          <div class="mode-name">Отладка стенда</div>
          <div class="mode-code">debug</div>
        </div>
        <button class="btn-neutral" onclick="location.href='/debug'">Настроить</button>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Управление</div>
    <div class="action-grid">
      <button class="btn-neutral" onclick="sendCommand('off')">Остановить</button>
      <button class="btn-danger" onclick="openEmergencyConfirm()">Остановить аварийно</button>
      <button class="btn-reset" onclick="sendCommand('reset')">Сбросить аварию</button>
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
      <input id="vminInput" type="number" step="0.1" min="0" max="1000" value="42.0">
    </div>

    <div class="form-row">
      <label>t, с</label>
      <input id="timeInput" type="number" step="1" min="1" max="86400" value="300">
    </div>

    <div class="form-row">
      <label>Температура, °C</label>
      <input id="tempInput" type="number" step="1" min="1" max="125" value="70">
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
      <input id="powerInput" type="number" step="1" min="1" max="100000" value="100">
    </div>

    <div class="form-row">
      <label>Imax, А</label>
      <input id="imaxInput" type="number" step="0.1" min="0.1" max="5.0" value="5.0">
    </div>

    <div class="form-row">
      <label>Vmin, В</label>
      <input id="pVminInput" type="number" step="0.1" min="0" max="1000" value="42.0">
    </div>

    <div class="form-row">
      <label>t, с</label>
      <input id="pTimeInput" type="number" step="1" min="1" max="86400" value="300">
    </div>

    <div class="form-row">
      <label>Температура, °C</label>
      <input id="pTempInput" type="number" step="1" min="1" max="125" value="70">
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

function clampNumberInput(id, minValue, maxValue, fallbackValue, integerOnly = false){
  const input = document.getElementById(id);
  let value = Number(input.value);
  if (!Number.isFinite(value)) {
    value = fallbackValue;
  }
  value = Math.max(minValue, Math.min(maxValue, value));
  if (integerOnly) {
    value = Math.round(value);
  }
  input.value = String(value);
  return input.value;
}

function updatePage(data){
  setText('current', Number(data.current).toFixed(2) + ' А');
  setText('voltage', Number(data.voltage).toFixed(2) + ' В');
  setText('power', Number(data.power).toFixed(1) + ' Вт');
  setText('temp', data.temperatureValid ? Number(data.temp).toFixed(1) + ' °C' : '--');

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

  let i = clampNumberInput('currentInput', 0.1, Number(document.getElementById('currentInput').max), 1.0);
  let vmin = clampNumberInput('vminInput', 0.0, 1000.0, 0.0);
  let t = clampNumberInput('timeInput', 1, 86400, 300, true);
  let temp = clampNumberInput('tempInput', 1.0, 125.0, 70.0);

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

  let p = clampNumberInput('powerInput', 1.0, 100000.0, 10.0);
  let imax = clampNumberInput('imaxInput', 0.1, Number(document.getElementById('imaxInput').max), 1.0);
  let vmin = clampNumberInput('pVminInput', 0.0, 1000.0, 0.0);
  let t = clampNumberInput('pTimeInput', 1, 86400, 300, true);
  let temp = clampNumberInput('pTempInput', 1.0, 125.0, 70.0);

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
.card{background:#1a212b;border:1px solid #2d3746;border-radius:8px;padding:16px;margin-bottom:14px}
.header{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.title{font-size:28px;font-weight:800;letter-spacing:.3px}
.back{color:#9db5d4;text-decoration:none;font-weight:800}
.section-title{font-size:22px;font-weight:800;margin-bottom:14px;color:#dfe8f7}
.field-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.field{background:#121820;border:1px solid #2b3443;border-radius:6px;padding:14px}
label{display:block;font-size:16px;font-weight:800;margin-bottom:6px;color:#fff}
.hint{min-height:38px;color:#9eacc0;font-size:14px;line-height:1.35;margin-bottom:10px}
input{width:100%;padding:13px;border-radius:6px;border:1px solid #354153;background:#0f151d;color:#fff;font-size:18px}
.switch-grid{display:grid;grid-template-columns:1fr;gap:10px}
.switch-row{display:flex;align-items:center;justify-content:space-between;gap:14px;background:#121820;border:1px solid #2b3443;border-radius:6px;padding:13px 14px}
.switch-copy{min-width:0}
.switch-title{font-size:16px;font-weight:800;color:#fff;margin-bottom:4px}
.switch-hint{color:#9eacc0;font-size:13px;line-height:1.3}
.switch{position:relative;display:inline-flex;align-items:center;width:58px;height:32px;flex:0 0 auto}
.switch input{position:absolute;opacity:0;width:1px;height:1px}
.switch-slider{position:absolute;inset:0;border-radius:999px;background:#596273;border:1px solid #687385;box-shadow:inset 0 1px 2px rgba(0,0,0,.28);transition:background .16s ease,border-color .16s ease}
.switch-slider:before{content:"";position:absolute;width:28px;height:28px;left:1px;top:1px;border-radius:50%;background:#f7f9fc;box-shadow:0 2px 8px rgba(0,0,0,.32);transition:transform .16s ease}
.switch input:checked + .switch-slider{background:#35c46b;border-color:#45d57b}
.switch input:checked + .switch-slider:before{transform:translateX(26px)}
.switch input:focus-visible + .switch-slider{outline:2px solid #9db5d4;outline-offset:3px}
.diag-table{width:100%;border-collapse:collapse;background:#121820;border:1px solid #2b3443;border-radius:6px;overflow:hidden}
.diag-table th,.diag-table td{padding:10px 12px;border-bottom:1px solid #2b3443;text-align:left}
.diag-table tr:last-child th,.diag-table tr:last-child td{border-bottom:none}
.diag-table th{width:48%;color:#9eacc0;font-size:13px;font-weight:800}
.diag-table td{color:#fff;font-size:18px;font-weight:800;word-break:break-word}
button{width:100%;border:none;border-radius:6px;padding:14px 12px;font-size:18px;font-weight:800;cursor:pointer;color:#fff}
button:active{transform:scale(.99)}
.btn-main{background:#188a47}
.btn-neutral{background:#596273}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.message{display:none;background:#121820;border:1px solid #334054;border-radius:8px;padding:12px;color:#dbe5f4;font-size:15px;line-height:1.35;margin-bottom:14px}
@media(max-width:640px){
  .title{font-size:24px}
  .field-grid{grid-template-columns:1fr}
  .diag-table th,.diag-table td{display:block;width:100%}
  .diag-table th{padding-bottom:4px}
  .diag-table td{padding-top:0}
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
        <input id="kpI" type="number" step="0.01" min="0" max="20">
      </div>
      <div class="field">
        <label for="kiI">kiI</label>
        <div class="hint">I-часть тока: постепенно дотягивает PWM, пока ток отличается от заданного.</div>
        <input id="kiI" type="number" step="0.01" min="0" max="50">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">P = const</div>
    <div class="field-grid">
      <div class="field">
        <label for="kpP">kpP</label>
        <div class="hint">P-часть мощности: реагирует на изменение ошибки по мощности.</div>
        <input id="kpP" type="number" step="0.001" min="0" max="20">
      </div>
      <div class="field">
        <label for="kiP">kiP</label>
        <div class="hint">I-часть мощности: плавно дотягивает нагрузку до заданной мощности.</div>
        <input id="kiP" type="number" step="0.001" min="0" max="50">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Ограничение изменения PWM</div>
    <div class="field-grid">
      <div class="field">
        <label for="stepUp">PWM_STEP_UP_MAX</label>
        <div class="hint">Максимальный шаг роста PWM за один цикл управления. Ограничивает резкое открытие MOSFET.</div>
        <input id="stepUp" type="number" step="0.1" min="0.01" max="255">
      </div>
      <div class="field">
        <label for="stepDown">PWM_STEP_DOWN_MAX</label>
        <div class="hint">Максимальный шаг снижения PWM за один цикл. Позволяет быстрее уменьшать ток при превышении.</div>
        <input id="stepDown" type="number" step="0.1" min="0.01" max="255">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Переключатели</div>
    <div class="switch-grid">
      <div class="switch-row">
        <div class="switch-copy">
          <div class="switch-title">Выход нагрузки</div>
          <div class="switch-hint">Реальный PWM на MOSFET. В выключенном состоянии выход держится в нуле.</div>
        </div>
        <label class="switch" aria-label="Выход нагрузки">
          <input id="loadOutputEnabled" type="checkbox">
          <span class="switch-slider"></span>
        </label>
      </div>
      <div class="switch-row">
        <div class="switch-copy">
          <div class="switch-title">Защита по Vmin</div>
          <div class="switch-hint">Остановка теста при просадке входного напряжения ниже заданного Vmin.</div>
        </div>
        <label class="switch" aria-label="Защита по Vmin">
          <input id="voltageProtectionEnabled" type="checkbox">
          <span class="switch-slider"></span>
        </label>
      </div>
      <div class="switch-row">
        <div class="switch-copy">
          <div class="switch-title">Защита по температуре</div>
          <div class="switch-hint">Требует исправный DS18B20 и останавливает тест при перегреве. Можно выключить для работы без датчика.</div>
        </div>
        <label class="switch" aria-label="Защита по температуре">
          <input id="temperatureProtectionEnabled" type="checkbox">
          <span class="switch-slider"></span>
        </label>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">PWM / MOSFET</div>
    <div class="field-grid">      <div class="field">
        <label for="pwmMin">Минимальный PWM нагрузки</label>
        <div class="hint">Минимальное ненулевое значение PWM. Команда остановки всё равно записывает 0.</div>
        <input id="pwmMin" type="number" step="1" min="0" max="65535">
      </div>
      <div class="field">
        <label for="pwmMax">Максимальный PWM нагрузки</label>
        <div class="hint">Верхняя граница PWM. Для 8-битного режима обычно 255.</div>
        <input id="pwmMax" type="number" step="1" min="0" max="65535">
      </div>
      <div class="field">
        <label for="pwmFreq">Частота PWM, Гц</label>
        <div class="hint">Частота управления MOSFET. При изменении выход нагрузки сбрасывается.</div>
        <input id="pwmFreq" type="number" step="1" min="1" max="40000">
      </div>
      <div class="field">
        <label for="pwmResolution">Разрешение PWM, бит</label>
        <div class="hint">Разрядность PWM. При изменении выход нагрузки сбрасывается.</div>
        <input id="pwmResolution" type="number" step="1" min="1" max="16">
      </div>
      <div class="field">
        <label for="maxTestCurrent">Предельный ток настройки, А</label>
        <div class="hint">Верхняя граница для задания тока в I = const и Imax в P = const.</div>
        <input id="maxTestCurrent" type="number" step="0.1" min="0.1" max="1000">
      </div>
      <div class="field">
        <label for="overCurrentFactor">Коэффициент аварии по току</label>
        <div class="hint">Множитель для аварии OVER_CURRENT в режиме I = const.</div>
        <input id="overCurrentFactor" type="number" step="0.01" min="1" max="10">
      </div>
      <div class="field">
        <label for="overCurrentConfirmCount">Подтверждение аварии по току</label>
        <div class="hint">Сколько циклов подряд ток должен быть выше предела перед аварией.</div>
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
        <label for="currentK">Коэффициент тока currentK</label>
        <div class="hint">Коэффициент пересчёта разницы ADC в ток. Удобно менять при подборе шунта и усилителя.</div>
        <input id="currentK" type="number" step="0.00001" min="0.00001" max="10">
      </div>
      <div class="field">
        <label for="voltageK">Коэффициент напряжения voltageK</label>
        <div class="hint">Коэффициент делителя входного напряжения. Увеличить, если ESP32 показывает меньше реального.</div>
        <input id="voltageK" type="number" step="0.001" min="0.001" max="500">
      </div>
      <div class="field">
        <label for="adcRefVoltage">Опорное напряжение ADC, В</label>
        <div class="hint">Напряжение, через которое сырые значения ADC пересчитываются в вольты.</div>
        <input id="adcRefVoltage" type="number" step="0.001" min="0.1" max="5">
      </div>
      <div class="field">
        <label for="adcMaxValue">Максимальный код ADC</label>
        <div class="hint">Верхнее сырое значение ADC. Для 12 бит обычно 4095.</div>
        <input id="adcMaxValue" type="number" step="1" min="1" max="65535">
      </div>
      <div class="field">
        <label for="analogAverageSamples">Усреднение ADC</label>
        <div class="hint">Сколько новых пар P/N читается при одном обновлении датчиков.</div>
        <input id="analogAverageSamples" type="number" step="1" min="1" max="512">
      </div>
      <div class="field">
        <label for="movingAverageSamples">Глубина скользящего среднего</label>
        <div class="hint">Размер буфера скользящего среднего для сырых пар P/N.</div>
        <input id="movingAverageSamples" type="number" step="1" min="1" max="512">
      </div>
      <div class="field">
        <label for="currentZeroSamples">Измерения автонуля тока</label>
        <div class="hint">Сколько измерений используется для автокалибровки нуля перед стартом режима.</div>
        <input id="currentZeroSamples" type="number" step="1" min="1" max="500">
      </div>
      <div class="field">
        <label for="currentZeroStabilityRaw">Допустимый разброс автонуля</label>
        <div class="hint">Максимальный сырой разброс сигнала при автокалибровке нуля тока.</div>
        <input id="currentZeroStabilityRaw" type="number" step="1" min="1" max="4095">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="section-title">Диагностика схемы</div>
    <table class="diag-table">
      <tbody>
        <tr><th>Ток</th><td id="dbgCurrent">--</td></tr>
        <tr><th>Напряжение</th><td id="dbgVoltage">--</td></tr>
        <tr><th>Мощность</th><td id="dbgPower">--</td></tr>
        <tr><th>Температура</th><td id="dbgTemp">--</td></tr>
        <tr><th>PWM</th><td id="dbgPwm">--</td></tr>
        <tr><th>Режим</th><td id="dbgMode">--</td></tr>
        <tr><th>Сырая разница тока</th><td id="dbgCurrentDiff">--</td></tr>
        <tr><th>Сырой вход тока P</th><td id="dbgCurrentRawP">--</td></tr>
        <tr><th>Сырой вход тока N</th><td id="dbgCurrentRawN">--</td></tr>
        <tr><th>Ноль тока raw</th><td id="dbgCurrentZero">--</td></tr>
        <tr><th>Сырая разница напряжения</th><td id="dbgVoltageDiff">--</td></tr>
        <tr><th>Сырой вход напряжения P</th><td id="dbgVoltageRawP">--</td></tr>
        <tr><th>Сырой вход напряжения N</th><td id="dbgVoltageRawN">--</td></tr>
      </tbody>
    </table>
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
  document.getElementById('loadOutputEnabled').checked = Boolean(data.loadOutputEnabled);
  document.getElementById('voltageProtectionEnabled').checked = Boolean(data.voltageProtectionEnabled);
  document.getElementById('temperatureProtectionEnabled').checked = Boolean(data.temperatureProtectionEnabled);
  document.getElementById('pwmMin').value = Number(data.pwmMin);
  document.getElementById('pwmMax').value = Number(data.pwmMax);
  document.getElementById('pwmFreq').value = Number(data.pwmFreq);
  document.getElementById('pwmResolution').value = Number(data.pwmResolution);
  setPwmInputLimits(Number(data.pwmResolution));
  document.getElementById('currentK').value = Number(data.currentK).toFixed(5);
  document.getElementById('voltageK').value = Number(data.voltageK).toFixed(3);
  document.getElementById('adcRefVoltage').value = Number(data.adcRefVoltage).toFixed(3);
  document.getElementById('adcMaxValue').value = Number(data.adcMaxValue).toFixed(0);
  document.getElementById('analogAverageSamples').value = Number(data.analogAverageSamples);
  document.getElementById('movingAverageSamples').value = Number(data.movingAverageSamples);
  document.getElementById('currentZeroSamples').value = Number(data.currentZeroSamples);
  document.getElementById('currentZeroStabilityRaw').value = Number(data.currentZeroStabilityRaw).toFixed(1);
  updateDiagnostics(data);
}

function pwmMaxForResolution(bits){
  let value = Number(bits);
  if (!Number.isFinite(value)) {
    value = 8;
  }
  value = Math.max(1, Math.min(16, Math.floor(value)));
  return value >= 16 ? 65535 : Math.pow(2, value) - 1;
}

function setPwmInputLimits(bits){
  const maxValue = pwmMaxForResolution(bits);
  const minInput = document.getElementById('pwmMin');
  const maxInput = document.getElementById('pwmMax');
  minInput.max = String(maxValue);
  maxInput.max = String(maxValue);
  if (Number(minInput.value) > maxValue) {
    minInput.value = maxValue;
  }
  if (Number(maxInput.value) > maxValue) {
    maxInput.value = maxValue;
  }
}

function setDiag(id, value){
  document.getElementById(id).innerText = value;
}

function updateDiagnostics(data){
  setDiag('dbgCurrent', Number(data.debugCurrent).toFixed(3) + ' А');
  setDiag('dbgVoltage', Number(data.debugVoltage).toFixed(2) + ' В');
  setDiag('dbgPower', Number(data.debugPower).toFixed(1) + ' Вт');
  setDiag('dbgTemp', data.temperatureValid ? Number(data.debugTemp).toFixed(1) + ' °C' : '--');
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
  const input = document.getElementById(id);
  if (input.type === 'checkbox') {
    return input.checked ? '1' : '0';
  }
  return encodeURIComponent(input.value);
}

function clampDebugInput(id, minValue, maxValue, fallbackValue, integerOnly = false){
  const input = document.getElementById(id);
  let value = Number(input.value);
  if (!Number.isFinite(value)) {
    value = fallbackValue;
  }
  value = Math.max(minValue, Math.min(maxValue, value));
  if (integerOnly) {
    value = Math.round(value);
  }
  input.value = String(value);
}

function normalizeDebugInputs(){
  clampDebugInput('kpI', 0.0, 20.0, 0.0);
  clampDebugInput('kiI', 0.0, 50.0, 0.0);
  clampDebugInput('kpP', 0.0, 20.0, 0.0);
  clampDebugInput('kiP', 0.0, 50.0, 0.0);
  clampDebugInput('stepUp', 0.01, 255.0, 1.0);
  clampDebugInput('stepDown', 0.01, 255.0, 1.0);
  clampDebugInput('fanOnTemp', 0.0, 120.0, 60.0);
  clampDebugInput('maxTestCurrent', 0.1, 1000.0, 5.0);
  clampDebugInput('overCurrentFactor', 1.0, 10.0, 1.25);
  clampDebugInput('overCurrentConfirmCount', 1, 100, 3, true);
  clampDebugInput('pwmResolution', 1, 16, 8, true);
  setPwmInputLimits(document.getElementById('pwmResolution').value);
  const pwmMax = pwmMaxForResolution(document.getElementById('pwmResolution').value);
  clampDebugInput('pwmMin', 0, pwmMax, 0, true);
  clampDebugInput('pwmMax', 0, pwmMax, pwmMax, true);
  clampDebugInput('pwmFreq', 1, 40000, 5000, true);
  clampDebugInput('currentK', 0.00001, 10.0, 0.03931);
  clampDebugInput('voltageK', 0.001, 500.0, 1.0);
  clampDebugInput('adcRefVoltage', 0.1, 5.0, 3.3);
  clampDebugInput('adcMaxValue', 1, 65535, 4095, true);
  clampDebugInput('analogAverageSamples', 1, 512, 10, true);
  clampDebugInput('movingAverageSamples', 1, 512, 10, true);
  clampDebugInput('currentZeroSamples', 1, 500, 50, true);
  clampDebugInput('currentZeroStabilityRaw', 1.0, 4095.0, 40.0);
}

function saveDebug(){
  normalizeDebugInputs();

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
            '&temperatureProtectionEnabled=' + valueParam('temperatureProtectionEnabled') +
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
  document.getElementById('pwmResolution').addEventListener('input', function(){
    setPwmInputLimits(this.value);
  });
  loadDebug();
  setInterval(refreshDiagnostics, 1000);
};
</script>
</body>
</html>
)rawliteral";
