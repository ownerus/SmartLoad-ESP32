from __future__ import annotations

from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
WEB_DIR = ROOT_DIR / "web"
OUTPUT_PATH = ROOT_DIR / "libraries" / "SmartLoadWeb" / "SmartLoadWeb.h"
RAW_DELIMITER = "rawliteral"


def read_web_file(name: str) -> str:
    text = (WEB_DIR / name).read_text(encoding="utf-8")
    if f"){RAW_DELIMITER}\"" in text:
        raise ValueError(f"{name} contains the raw string delimiter")
    return text.rstrip() + "\n"


def render_constant(name: str, html: str) -> str:
    return f'const char {name}[] PROGMEM = R"{RAW_DELIMITER}(\n{html}){RAW_DELIMITER}";'


def main() -> None:
    index_html = read_web_file("index.html")
    debug_html = read_web_file("debug.html")
    header = "\n\n".join(
        [
            "#pragma once\n\n#include <Arduino.h>\n#include <pgmspace.h>",
            render_constant("INDEX_HTML", index_html),
            render_constant("DEBUG_HTML", debug_html),
        ]
    )
    OUTPUT_PATH.write_text(header + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
