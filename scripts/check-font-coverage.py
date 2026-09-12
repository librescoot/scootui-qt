#!/usr/bin/env python3
"""Fail if a UI string uses a character no bundled font has.

A missing glyph draws as a gap on the scooter but can look fine on the
desktop, where Qt falls back to a system font. Scans string literals in
src/ and qml/ (comments skipped); run via scripts/subset-fonts.sh.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from fontTools.ttLib import TTFont

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
SUBSET_DIR = REPO_ROOT / "assets" / "fonts" / "subset"
MATERIAL_QML = REPO_ROOT / "qml" / "widgets" / "components" / "MaterialIcon.qml"

SCAN_ROOTS = ("src", "qml")
# Simulator is dev-only and uses an emoji no bundled font carries.
SCAN_EXCLUDE_DIRS = {Path("qml") / "simulator"}
SOURCE_SUFFIXES = {".cpp", ".h", ".qml"}


def bundled_codepoints() -> set[int]:
    covered: set[int] = set()
    for path in sorted(SUBSET_DIR.glob("*")):
        if path.suffix.lower() in {".ttf", ".otf"}:
            covered |= set(TTFont(path).getBestCmap().keys())
    # MaterialIcon codepoints are escapes, not literal characters.
    if MATERIAL_QML.exists():
        for token in re.findall(r"0x[0-9a-fA-F]+|\\u[0-9a-fA-F]{4}",
                                MATERIAL_QML.read_text(encoding="utf-8")):
            covered.add(int(token, 16) if token.startswith("0x") else int(token[2:], 16))
    return covered


def strip_comments_and_collect_strings(text: str):
    """Yield (line_number, decoded_string); regexes trip on URLs and escaped quotes."""
    line = 1
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "\n":
            line += 1
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                if text[i] == "\n":
                    line += 1
                i += 1
            i += 2
            continue
        if ch not in ('"', "'"):
            i += 1
            continue
        quote = ch
        start_line = line
        i += 1
        out = []
        while i < n and text[i] != quote:
            c = text[i]
            if c == "\\" and i + 1 < n:
                nxt = text[i + 1]
                if nxt == "u" and i + 5 < n and re.fullmatch(r"[0-9a-fA-F]{4}", text[i + 2:i + 6]):
                    out.append(chr(int(text[i + 2:i + 6], 16)))
                    i += 6
                    continue
                if nxt == "x" and i + 3 < n and re.fullmatch(r"[0-9a-fA-F]{2}", text[i + 2:i + 4]):
                    out.append(chr(int(text[i + 2:i + 4], 16)))
                    i += 4
                    continue
                out.append(nxt)
                i += 2
                continue
            if c == "\n":
                line += 1
            out.append(c)
            i += 1
        i += 1  # closing quote
        yield start_line, "".join(out)


def main() -> int:
    covered = bundled_codepoints()
    uncovered: dict[str, list[tuple[str, int]]] = {}

    for root_name in SCAN_ROOTS:
        root = REPO_ROOT / root_name
        for path in sorted(root.rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            rel = path.relative_to(REPO_ROOT)
            if any(rel.is_relative_to(excluded) for excluded in SCAN_EXCLUDE_DIRS):
                continue
            text = path.read_text(encoding="utf-8")
            for line, literal in strip_comments_and_collect_strings(text):
                for ch in literal:
                    if ord(ch) > 0x7F and ord(ch) not in covered:
                        uncovered.setdefault(ch, []).append((str(rel), line))

    if not uncovered:
        print(f"font coverage OK ({len(covered)} codepoints bundled)")
        return 0

    print("font coverage FAILED — characters used in UI strings with no bundled glyph:", file=sys.stderr)
    for ch, sites in sorted(uncovered.items(), key=lambda kv: ord(kv[0])):
        print(f"\n  U+{ord(ch):04X} {ch!r}  x{len(sites)}", file=sys.stderr)
        for site, line in sites[:8]:
            print(f"      {site}:{line}", file=sys.stderr)
        if len(sites) > 8:
            print("      ...", file=sys.stderr)
    print("\nAdd the codepoint to scripts/subset-fonts.sh and regenerate, or "
          "replace it in the copy.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
