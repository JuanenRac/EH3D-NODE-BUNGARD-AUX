#!/usr/bin/env python3
# EH3D-NODE-BUNGARD-AUX - tools/render_answers.py
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
"""Draws what the node answers on the network (the status, a measure and the line of a point of the map of the table) in the look of the desktop programs (the Forge
theme). The words are the formats that the host tests check in core/protocol.hpp; this is a drawing, not a capture from a board.
    python tools/render_answers.py          # writes images/screenshots/answers.svg
    python tools/render_answers.py --png    # also renders answers.png with Microsoft Edge
"""
import subprocess
import sys
import tempfile
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "images" / "screenshots"
BG, PANEL, LINE, TEXT, DIM, ACCENT, OK = "#14110e", "#1d1914", "#6b4a12", "#f2e8d5", "#9a8c74", "#ffb000", "#7bd88f"

BLOCKS = [
    ("GET /api/status", [
        '{"node":"bungard-aux","version":"0.0.1","camera":true,"laser":true,',
        ' "rpm_sensor":true,"rpm":23998,"temp_c":41.3,"heat":"ok",',
        ' "protect_open":false,"uptime_s":90}',
    ]),
    ("POST /api/measure?samples=7   (Authorization: Bearer ...)", [
        '{"ok":true,"why":"ok","distance_mm":100.012,"sigma_mm":0.021,',
        ' "asked":7,"used":6,"rejected":1}',
    ]),
    ("a measure that is refused", [
        '{"ok":false,"why":"too_noisy","distance_mm":100.000,"sigma_mm":0.254,',
        ' "asked":7,"used":7,"rejected":0}',
    ]),
    ("the line the desktop programs read as a point of the map", [
        "Z 12.500 40.000 -100.0120",
    ]),
]


def svg() -> str:
    w, h = 1280, 800
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" font-family="Consolas, Menlo, monospace">',
           f'<rect width="{w}" height="{h}" fill="{BG}"/>',
           f'<text x="40" y="58" font-size="30" fill="{TEXT}" font-family="Segoe UI, sans-serif">EH3D</text>',
           f'<text x="118" y="58" font-size="30" fill="{ACCENT}" font-family="Segoe UI, sans-serif">BUNGARD AUX</text>',
           f'<line x1="40" y1="78" x2="{w - 40}" y2="78" stroke="{LINE}" stroke-width="1"/>']
    y = 120
    for title, lines in BLOCKS:
        height = 56 + 30 * len(lines)
        out.append(f'<rect x="40" y="{y}" width="{w - 80}" height="{height}" rx="6" fill="{PANEL}" stroke="{LINE}" stroke-width="1"/>')
        out.append(f'<text x="64" y="{y + 32}" font-size="17" fill="{ACCENT}">{escape(title)}</text>')
        for i, line in enumerate(lines):
            colour = OK if line.startswith("Z ") else TEXT
            out.append(f'<text x="64" y="{y + 66 + 30 * i}" font-size="22" fill="{colour}" xml:space="preserve">{escape(line)}</text>')
        y += height + 22
    out.append(f'<text x="40" y="{h - 28}" font-size="15" fill="{DIM}">the formats of core/protocol.hpp, as the host tests check them; a drawing, not a capture from a board</text>')
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "answers.svg").write_text(svg(), encoding="utf-8", newline="\n")
    print("written answers.svg")
    if "--png" in sys.argv:
        edge = next((p for p in (r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe", r"C:\Program Files\Microsoft\Edge\Application\msedge.exe") if Path(p).exists()), None)
        if not edge:
            print("Microsoft Edge not found")
            return 1
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([edge, "--headless=new", f"--user-data-dir={tmp}", "--hide-scrollbars", "--window-size=1280,800",
                            f"--screenshot={OUT / 'answers.png'}", (OUT / "answers.svg").as_uri()], capture_output=True, check=False)
        print("rendered answers.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
