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
STATIC_DIR = BASE_DIR / "static"
INDEX_PATH = STATIC_DIR / "index.html"
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
STATIC_CONTENT_TYPES = {
    ".css": "text/css; charset=utf-8",
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
}



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
            self.send_file(INDEX_PATH)
            return

        if parsed.path.startswith("/static/"):
            self.send_static(parsed.path)
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

    def send_static(self, path: str) -> None:
        relative_path = path.removeprefix("/static/").strip("/")
        file_path = (STATIC_DIR / relative_path).resolve()

        if not file_path.is_relative_to(STATIC_DIR.resolve()):
            self.send_error(HTTPStatus.NOT_FOUND, "Not found")
            return

        self.send_file(file_path)

    def send_file(self, file_path: Path) -> None:
        if not file_path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND, "Not found")
            return

        content_type = STATIC_CONTENT_TYPES.get(file_path.suffix.lower(), "application/octet-stream")
        body = file_path.read_bytes()
        self.send_response(HTTPStatus.OK)
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
