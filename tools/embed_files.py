#!/usr/bin/env python3
"""Genera src/embedded_data.h incorporando i font nel file eseguibile.

Uso: python3 tools/embed_files.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = [
    ("FONT_UI", "assets/fonts/ShareTechMono-Regular.ttf"),
    ("FONT_TERM", "assets/fonts/VT323-Regular.ttf"),
]

out = ["// File generato da tools/embed_files.py - non modificare a mano.", "#pragma once", ""]
for name, rel in FILES:
    with open(os.path.join(ROOT, rel), "rb") as f:
        data = f.read()
    out.append(f"// {rel}")
    out.append(f"static const unsigned int {name}_SIZE = {len(data)};")
    out.append(f"static const unsigned char {name}_DATA[] = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append("")

with open(os.path.join(ROOT, "src", "embedded_data.h"), "w") as f:
    f.write("\n".join(out))
print("ok")
