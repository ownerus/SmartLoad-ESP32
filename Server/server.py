from __future__ import annotations

import argparse
import json
import math
import sqlite3
import threading
from datetime import datetime, timedelta
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote_plus, urlparse


BASE_DIR = Path(__file__).resolve().parent
DATA_DIR = BASE_DIR / "data"
DB_PATH = DATA_DIR / "telemetry.sqlite3"
MAX_POST_BYTES = 64 * 1024

TELEMETRY_FIELDS = [
    "received_at",
    "client_time",
    "event",
    "current_A",
    "voltage_V",
    "power_W",
    "temp_C",
    "pwm",
    "elapsed_s",
    "mode",
    "info",
]

NUMERIC_FIELDS = {
    "current_A",
    "voltage_V",
    "power_W",
    "temp_C",
    "elapsed_s",
}

MAX_CHART_POINTS = 8000


INDEX_HTML = r"""<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SmartLoad Telemetry</title>
<style>
*{box-sizing:border-box}
body{margin:0;background:#11161d;color:#eef2f7;font-family:Arial,sans-serif}
.wrap{max-width:1180px;margin:0 auto;padding:16px}
.top{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-bottom:14px}
h1{font-size:28px;margin:0}
.status{color:#98a6b8;font-size:14px}
.cards{display:grid;grid-template-columns:repeat(6,1fr);gap:10px;margin-bottom:14px}
.card{background:#1a212b;border:1px solid #2d3746;border-radius:8px;padding:12px}
.label{color:#98a6b8;font-size:13px;margin-bottom:6px}
.value{font-size:23px;font-weight:800;white-space:nowrap}
.time-value{font-size:18px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.chart{background:#1a212b;border:1px solid #2d3746;border-radius:8px;padding:12px}
.chart h2{font-size:17px;margin:0 0 8px}
canvas{width:100%;height:240px;display:block}
.chart-controls{display:flex;align-items:center;justify-content:space-between;gap:14px;background:#1a212b;border:1px solid #2d3746;border-radius:8px;padding:12px;margin-bottom:14px}
.chart-controls-title{font-size:17px;font-weight:800}
.chart-controls-subtitle{color:#9eacc0;font-size:13px;margin-top:3px}
.period-select{position:relative}
.period-button{display:flex;align-items:center;gap:10px;min-width:190px;background:#121820;color:#eef2f7;border:1px solid #354153;border-radius:6px;padding:11px 12px;font-size:15px;font-weight:700;cursor:pointer}
.period-button:hover{border-color:#596273;background:#1a212b}
.period-arrow{margin-left:auto;border-left:5px solid transparent;border-right:5px solid transparent;border-top:6px solid #d7e1ef}
.period-menu{display:none;position:absolute;right:0;top:calc(100% + 6px);width:210px;background:#1a212b;border:1px solid #354153;border-radius:8px;box-shadow:0 8px 24px rgba(0,0,0,.25);padding:6px;z-index:10}
.period-select.open .period-menu{display:block}
.period-option{width:100%;border:none;background:transparent;color:#dbe5f4;text-align:left;padding:10px 11px;border-radius:6px;font-size:14px;cursor:pointer}
.period-option:hover{background:#2e3847}
.period-option.active{background:#596273;color:#fff}
@media(max-width:980px){.cards{grid-template-columns:repeat(3,1fr)}}
@media(max-width:820px){.chart-controls{align-items:stretch;flex-direction:column}.period-button{width:100%}.period-menu{left:0;right:auto;width:100%}.grid{grid-template-columns:1fr}}
@media(max-width:560px){.cards{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <h1>SmartLoad ESP32 Telemetry</h1>
    <div class="status" id="status">ожидание данных...</div>
  </div>

  <div class="cards">
    <div class="card"><div class="label">Ток</div><div class="value" id="current">-- A</div></div>
    <div class="card"><div class="label">Напряжение</div><div class="value" id="voltage">-- V</div></div>
    <div class="card"><div class="label">Мощность</div><div class="value" id="power">-- W</div></div>
    <div class="card"><div class="label">Температура</div><div class="value" id="temp">-- C</div></div>
    <div class="card"><div class="label">PWM / режим</div><div class="value" id="pwm">--</div></div>
    <div class="card"><div class="label">Время</div><div class="value time-value" id="latestTime">--:--:--</div></div>
  </div>

  <div class="chart-controls">
    <div>
      <div class="chart-controls-title">Период графиков</div>
      <div class="chart-controls-subtitle">Выберите, какой участок телеметрии показать ниже</div>
    </div>
    <div class="period-select" id="periodSelect">
      <button class="period-button" id="periodButton" type="button">
        <span id="periodLabel">За 10 минут</span>
        <span class="period-arrow"></span>
      </button>
      <div class="period-menu" id="periodMenu">
        <button class="period-option" type="button" data-range="1m">За 1 минуту</button>
        <button class="period-option active" type="button" data-range="10m">За 10 минут</button>
        <button class="period-option" type="button" data-range="all">За всё время</button>
      </div>
    </div>
  </div>

  <div class="grid">
    <div class="chart"><h2>Ток, А</h2><canvas id="chartCurrent"></canvas></div>
    <div class="chart"><h2>Напряжение, В</h2><canvas id="chartVoltage"></canvas></div>
    <div class="chart"><h2>Мощность, Вт</h2><canvas id="chartPower"></canvas></div>
    <div class="chart"><h2>Температура, °C</h2><canvas id="chartTemp"></canvas></div>
  </div>
</div>

<script>
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
</script>
</body>
</html>
"""


def now_text() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def parse_received_at(value: str) -> datetime:
    for fmt in ("%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%d %H:%M:%S"):
        try:
            return datetime.strptime(value, fmt)
        except ValueError:
            pass

    raise ValueError(f"Bad received_at value: {value}")


def first_value(data: dict[str, str], *names: str) -> str:
    for name in names:
        value = data.get(name)
        if value is not None and value != "":
            return str(value)
    return ""


def to_float_text(value: str) -> str:
    if value == "":
        return ""
    try:
        number = float(str(value).replace(",", "."))
    except ValueError:
        return ""
    if not math.isfinite(number):
        return ""
    return str(number)


def parse_serial_line(line: str) -> dict[str, str]:
    parts = line.split(",", 9)
    while len(parts) < 10:
        parts.append("")
    return {
        "client_time": parts[0].strip(),
        "event": parts[1].strip(),
        "current_A": parts[2].strip(),
        "voltage_V": parts[3].strip(),
        "power_W": parts[4].strip(),
        "temp_C": parts[5].strip(),
        "pwm": parts[6].strip(),
        "elapsed_s": parts[7].strip(),
        "mode": parts[8].strip(),
        "info": ",".join(parts[9:]).strip(),
    }


def normalize_row(data: dict[str, object]) -> dict[str, str]:
    text_data = {str(k): "" if v is None else str(v) for k, v in data.items()}

    if text_data.get("line"):
        row = parse_serial_line(unquote_plus(text_data["line"]))
    else:
        row = {
            "client_time": first_value(text_data, "client_time", "timestamp", "time"),
            "event": first_value(text_data, "event"),
            "current_A": first_value(text_data, "current_A", "current", "i"),
            "voltage_V": first_value(text_data, "voltage_V", "voltage", "u"),
            "power_W": first_value(text_data, "power_W", "power", "p"),
            "temp_C": first_value(text_data, "temp_C", "temperature", "temp", "t"),
            "pwm": first_value(text_data, "pwm"),
            "elapsed_s": first_value(text_data, "elapsed_s", "elapsed"),
            "mode": first_value(text_data, "mode"),
            "info": first_value(text_data, "info", "message"),
        }

    row["received_at"] = now_text()

    for field in NUMERIC_FIELDS:
        row[field] = to_float_text(row.get(field, ""))

    if row["power_W"] == "" and row["current_A"] != "" and row["voltage_V"] != "":
        row["power_W"] = str(float(row["current_A"]) * float(row["voltage_V"]))

    return {field: row.get(field, "") for field in TELEMETRY_FIELDS}


class TelemetryStore:
    def __init__(self, db_path: Path) -> None:
        self.db_path = db_path
        self.lock = threading.Lock()
        self.total_rows = 0
        self._init_db()
        self.refresh_total_rows()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self) -> None:
        self.db_path.parent.mkdir(parents=True, exist_ok=True)

        with self._connect() as conn:
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    received_at TEXT NOT NULL,
                    client_time TEXT,
                    event TEXT,
                    current_A REAL,
                    voltage_V REAL,
                    power_W REAL,
                    temp_C REAL,
                    pwm INTEGER,
                    elapsed_s REAL,
                    mode TEXT,
                    info TEXT
                )
                """
            )
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_telemetry_received_at ON telemetry(received_at)"
            )
            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_telemetry_event ON telemetry(event)"
            )

    def refresh_total_rows(self) -> None:
        with self._connect() as conn:
            self.total_rows = conn.execute("SELECT COUNT(*) FROM telemetry").fetchone()[0]

    def append(self, row: dict[str, str]) -> None:
        with self.lock:
            self._insert_row(row)
            self.total_rows += 1

    def _insert_row(self, row: dict[str, str]) -> None:
        with self._connect() as conn:
            conn.execute(
                """
                INSERT INTO telemetry (
                    received_at, client_time, event, current_A, voltage_V, power_W,
                    temp_C, pwm, elapsed_s, mode, info
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    row["received_at"],
                    row["client_time"],
                    row["event"],
                    self.to_optional_float(row["current_A"]),
                    self.to_optional_float(row["voltage_V"]),
                    self.to_optional_float(row["power_W"]),
                    self.to_optional_float(row["temp_C"]),
                    self.to_optional_int(row["pwm"]),
                    self.to_optional_float(row["elapsed_s"]),
                    row["mode"],
                    row["info"],
                ),
            )

    def latest(self, minutes: int | None = None) -> list[dict[str, str]]:
        with self.lock:
            with self._connect() as conn:
                rows = self._select_chart_rows(conn, minutes)

        return [self._row_to_dict(row) for row in rows]

    def _select_chart_rows(
        self,
        conn: sqlite3.Connection,
        minutes: int | None,
    ) -> list[sqlite3.Row]:
        fields = """
            received_at, client_time, event, current_A, voltage_V, power_W,
            temp_C, pwm, elapsed_s, mode, info
        """

        if minutes is None:
            stats = conn.execute(
                "SELECT COUNT(*) AS count_rows, MIN(id) AS min_id, MAX(id) AS max_id FROM telemetry"
            ).fetchone()
            where_sql = ""
            params: tuple[object, ...] = ()
        else:
            latest_text = conn.execute("SELECT MAX(received_at) FROM telemetry").fetchone()[0]

            if latest_text is None:
                return []

            latest_time = parse_received_at(latest_text)
            since = (latest_time - timedelta(minutes=minutes)).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            stats = conn.execute(
                """
                SELECT COUNT(*) AS count_rows, MIN(id) AS min_id, MAX(id) AS max_id
                FROM telemetry
                WHERE received_at >= ?
                """,
                (since,),
            ).fetchone()
            where_sql = "WHERE received_at >= ?"
            params = (since,)

        count_rows = int(stats["count_rows"] or 0)
        min_id = stats["min_id"]
        max_id = stats["max_id"]

        if count_rows == 0:
            return []

        if count_rows <= MAX_CHART_POINTS:
            return conn.execute(
                f"""
                SELECT {fields}
                FROM telemetry
                {where_sql}
                ORDER BY id
                """,
                params,
            ).fetchall()

        stride = max(1, math.ceil(count_rows / MAX_CHART_POINTS))

        if minutes is None:
            return conn.execute(
                f"""
                SELECT {fields}
                FROM telemetry
                WHERE ((id - ?) % ? = 0) OR id = ?
                ORDER BY id
                """,
                (min_id, stride, max_id),
            ).fetchall()

        return conn.execute(
            f"""
            SELECT {fields}
            FROM telemetry
            WHERE received_at >= ?
              AND (((id - ?) % ? = 0) OR id = ?)
            ORDER BY id
            """,
            (params[0], min_id, stride, max_id),
        ).fetchall()

    def _row_to_dict(self, row: sqlite3.Row) -> dict[str, str]:
        return {field: "" if row[field] is None else str(row[field]) for field in TELEMETRY_FIELDS}

    def to_optional_float(self, value: str) -> float | None:
        if value == "":
            return None
        return float(value)

    def to_optional_int(self, value: str) -> int | None:
        if value == "":
            return None
        return int(float(value))


STORE = TelemetryStore(DB_PATH)


class SmartLoadHandler(BaseHTTPRequestHandler):
    server_version = "SmartLoadTelemetry/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        print("[%s] %s" % (now_text(), fmt % args))

    def do_GET(self) -> None:
        parsed = urlparse(self.path)

        if parsed.path == "/":
            self.send_text(INDEX_HTML, "text/html; charset=utf-8")
            return

        if parsed.path in {"/telemetry", "/ingest", "/api/telemetry"}:
            params = self.parse_query(parsed.query)
            self.ingest(params)
            return

        if parsed.path == "/api/data":
            params = self.parse_query(parsed.query)
            range_name = params.get("range", "10m")
            minutes = self.range_to_minutes(range_name)
            self.send_json(
                {
                    "rows": STORE.latest(minutes),
                    "total_rows": STORE.total_rows,
                    "range": range_name,
                }
            )
            return

        if parsed.path == "/api/status":
            self.send_json(
                {
                    "ok": True,
                    "database": str(DB_PATH),
                    "table": "telemetry",
                    "total_rows": STORE.total_rows,
                }
            )
            return

        self.send_error(HTTPStatus.NOT_FOUND, "Not found")

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path not in {"/telemetry", "/ingest", "/api/telemetry"}:
            self.send_error(HTTPStatus.NOT_FOUND, "Not found")
            return

        length = self.safe_int(self.headers.get("Content-Length", "0"), 0)
        if length < 0:
            self.send_json({"ok": False, "error": "bad content length"}, HTTPStatus.BAD_REQUEST)
            return

        if length > MAX_POST_BYTES:
            self.send_json({"ok": False, "error": "payload too large"}, HTTPStatus.REQUEST_ENTITY_TOO_LARGE)
            return

        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        content_type = self.headers.get("Content-Type", "")

        if "application/json" in content_type:
            try:
                payload = json.loads(raw) if raw else {}
            except json.JSONDecodeError:
                self.send_json({"ok": False, "error": "bad json"}, HTTPStatus.BAD_REQUEST)
                return
        elif "application/x-www-form-urlencoded" in content_type:
            payload = self.parse_query(raw)
        elif raw.strip():
            payload = {"line": raw.strip()}
        else:
            payload = {}

        self.ingest(payload)

    def ingest(self, payload: dict[str, object]) -> None:
        row = normalize_row(payload)

        if not any(row[field] for field in ("current_A", "voltage_V", "temp_C", "event")):
            self.send_json({"ok": False, "error": "empty telemetry"}, HTTPStatus.BAD_REQUEST)
            return

        STORE.append(row)
        self.send_json({"ok": True, "row": row})

    def parse_query(self, query: str) -> dict[str, str]:
        parsed = parse_qs(query, keep_blank_values=True)
        return {key: values[-1] if values else "" for key, values in parsed.items()}

    def safe_int(self, value: str | None, default: int) -> int:
        try:
            return int(value or default)
        except ValueError:
            return default

    def range_to_minutes(self, range_name: str) -> int | None:
        if range_name == "1m":
            return 1
        if range_name == "10m":
            return 10
        return None

    def send_json(self, payload: object, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_text(self, text: str, content_type: str, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = text.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SmartLoad ESP32 telemetry HTTP server")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address")
    parser.add_argument("--port", default=8000, type=int, help="Bind port")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    server = ThreadingHTTPServer((args.host, args.port), SmartLoadHandler)
    print("SmartLoad telemetry server")
    print(f"Listening on http://{args.host}:{args.port}/")
    print(f"SQLite database: {DB_PATH}")
    print("SQL table: telemetry")
    print("Endpoints: GET/POST /telemetry, GET /api/data")
    server.serve_forever()


if __name__ == "__main__":
    main()
