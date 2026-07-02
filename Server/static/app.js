const charts = [
  {id:'chartCurrent', field:'current_A', color:'#65ef98'},
  {id:'chartVoltage', field:'voltage_V', color:'#7cc7ff'},
  {id:'chartPower', field:'power_W', color:'#ffca6a'},
  {id:'chartTemp', field:'temp_C', color:'#ff7c7c'}
];

const periods = {
  '1m': 'За 1 минуту',
  '10m': 'За 10 минут',
  'all': 'За всё время'
};

let selectedPeriod = '10m';
let refreshTimer = null;
let refreshInFlight = false;
let nextRefreshAt = 0;

function num(v){
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

function format(v, digits, unit){
  const n = num(v);
  return n === null ? '-- ' + unit : n.toFixed(digits) + ' ' + unit;
}

function resizeCanvas(canvas){
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.max(1, Math.floor(rect.width * dpr));
  canvas.height = Math.max(1, Math.floor(rect.height * dpr));
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return {ctx, width: rect.width, height: rect.height};
}

function formatTimeLabel(value, withDate){
  if (!value) return '';
  const parts = String(value).split(' ');
  const date = parts[0] || '';
  const time = (parts[1] || parts[0] || '').slice(0, 8);
  return withDate && date ? date.slice(5) + ' ' + time : time;
}

function drawChart(id, rows, field, color){
  const canvas = document.getElementById(id);
  const {ctx, width, height} = resizeCanvas(canvas);
  ctx.clearRect(0, 0, width, height);

  const padL = 48, padR = 12, padT = 10, padB = 48;
  const values = rows.map(r => num(r[field])).filter(v => v !== null);
  if (!values.length) {
    ctx.fillStyle = '#7f8da0';
    ctx.fillText('Нет данных', padL, 34);
    return;
  }

  const dataMin = Math.min(...values);
  const dataMax = Math.max(...values);
  let plotMin = dataMin;
  let plotMax = dataMax;
  if (plotMin === plotMax) {
    plotMin -= 1;
    plotMax += 1;
  }
  const span = plotMax - plotMin;
  plotMin -= span * 0.08;
  plotMax += span * 0.08;

  const plotW = width - padL - padR;
  const plotH = height - padT - padB;
  const y = v => padT + (plotMax - v) / (plotMax - plotMin) * plotH;
  const x = i => padL + (rows.length <= 1 ? 0 : i / (rows.length - 1) * plotW);

  ctx.strokeStyle = '#334257';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(padL, padT);
  ctx.lineTo(padL, padT + plotH);
  ctx.lineTo(padL + plotW, padT + plotH);
  ctx.stroke();

  ctx.fillStyle = '#91a0b3';
  ctx.font = '12px Arial';
  ctx.textAlign = 'right';
  ctx.fillText(dataMax.toFixed(2), padL - 7, padT + 10);
  ctx.fillText(dataMin.toFixed(2), padL - 7, padT + plotH);

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  let started = false;
  rows.forEach((row, i) => {
    const v = num(row[field]);
    if (v === null) return;
    if (!started) {
      ctx.moveTo(x(i), y(v));
      started = true;
    } else {
      ctx.lineTo(x(i), y(v));
    }
  });
  ctx.stroke();

  const firstDate = (rows[0].received_at || '').split(' ')[0];
  const lastDate = (rows[rows.length - 1].received_at || '').split(' ')[0];
  const withDate = firstDate && lastDate && firstDate !== lastDate;
  const timeMarks = [
    {index: 0, align: 'left'},
    {index: Math.floor((rows.length - 1) / 2), align: 'center'},
    {index: rows.length - 1, align: 'right'}
  ];

  ctx.fillStyle = '#18202b';
  ctx.fillRect(padL, padT + plotH + 1, plotW, padB - 2);
  ctx.strokeStyle = '#334257';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(padL, padT + plotH);
  ctx.lineTo(padL + plotW, padT + plotH);
  ctx.stroke();

  ctx.font = '12px Arial';
  ctx.fillStyle = '#c7d2e1';
  ctx.textBaseline = 'alphabetic';
  timeMarks.forEach(mark => {
    const xPos = x(mark.index);
    const label = formatTimeLabel(rows[mark.index].received_at, withDate);
    ctx.strokeStyle = '#435168';
    ctx.beginPath();
    ctx.moveTo(xPos, padT + plotH);
    ctx.lineTo(xPos, padT + plotH + 6);
    ctx.stroke();
    ctx.textAlign = mark.align;
    ctx.fillText(label, xPos, height - 11);
  });
}

function updateCards(row){
  document.getElementById('current').textContent = format(row.current_A, 2, 'A');
  document.getElementById('voltage').textContent = format(row.voltage_V, 2, 'V');
  document.getElementById('power').textContent = format(row.power_W, 1, 'W');
  document.getElementById('temp').textContent = format(row.temp_C, 1, 'C');
  const pwm = row.pwm === '' || row.pwm === undefined ? '--' : row.pwm;
  document.getElementById('pwm').textContent = pwm + ' / ' + (row.mode || '--');
  document.getElementById('latestTime').textContent =
    row.received_at ? row.received_at.split(' ').pop().slice(0, 8) : '--:--:--';
}

function togglePeriodMenu(){
  document.getElementById('periodSelect').classList.toggle('open');
}

function setPeriod(period){
  selectedPeriod = period;
  document.getElementById('periodLabel').textContent = periods[period];
  document.querySelectorAll('.period-option').forEach(button => {
    button.classList.toggle('active', button.dataset.range === period);
  });
  document.getElementById('periodSelect').classList.remove('open');
  nextRefreshAt = 0;
  refresh(true);
}

document.addEventListener('click', event => {
  const select = document.getElementById('periodSelect');
  if (!select.contains(event.target)) select.classList.remove('open');
});

function isPeriodMenuOpen(){
  return document.getElementById('periodSelect').classList.contains('open');
}

function scheduleRefresh(delay){
  if (refreshTimer) clearTimeout(refreshTimer);
  const periodMs = selectedPeriod === 'all' ? 5000 : 2000;
  const now = performance.now();

  if (delay !== undefined) {
    nextRefreshAt = now + delay;
  } else if (!nextRefreshAt || nextRefreshAt < now - periodMs) {
    nextRefreshAt = now + periodMs;
  } else {
    nextRefreshAt += periodMs;
  }

  const nextDelay = Math.max(0, nextRefreshAt - now);
  refreshTimer = setTimeout(() => refresh(false), nextDelay);
}

async function refresh(force){
  if (!force && isPeriodMenuOpen()) {
    scheduleRefresh(500);
    return;
  }

  if (refreshInFlight) {
    if (!force) scheduleRefresh();
    return;
  }

  refreshInFlight = true;
  try {
    const res = await fetch('/api/data?range=' + encodeURIComponent(selectedPeriod));
    const data = await res.json();
    const rows = data.rows || [];
    document.getElementById('status').textContent =
      rows.length ? ('строк: ' + data.total_rows + ', последняя: ' + rows[rows.length - 1].received_at)
                  : 'ожидание данных...';
    if (rows.length) updateCards(rows[rows.length - 1]);
    charts.forEach(c => drawChart(c.id, rows, c.field, c.color));
  } catch (e) {
    document.getElementById('status').textContent = 'ошибка соединения с сервером';
  } finally {
    refreshInFlight = false;
    scheduleRefresh();
  }
}

window.addEventListener('resize', () => refresh(true));
document.getElementById('periodButton').addEventListener('click', event => {
  event.stopPropagation();
  togglePeriodMenu();
});
document.querySelectorAll('.period-option').forEach(button => {
  button.addEventListener('click', event => {
    event.stopPropagation();
    setPeriod(button.dataset.range);
  });
});
refresh(true);
