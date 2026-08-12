# -*- coding: utf-8 -*-
"""Scan Source for non-ASCII characters in string literals (UI-facing)."""
import os
import re

root = os.path.join(os.path.dirname(__file__), "..", "Source")
root = os.path.normpath(root)
skip_dirs = {"MelatoninBlur", "shadows-main", "ScreenCaptureLite"}
pat = re.compile(r'"(?:\\.|[^"\\])*"')

results = []
for dirpath, dirs, files in os.walk(root):
    dirs[:] = [d for d in dirs if d not in skip_dirs]
    for f in files:
        if not f.endswith((".h", ".cpp", ".hpp")):
            continue
        path = os.path.join(dirpath, f)
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                lines = fh.readlines()
        except OSError:
            continue
        for i, line in enumerate(lines, 1):
            stripped = line.lstrip()
            is_comment = (
                stripped.startswith("//")
                or stripped.startswith("/*")
                or stripped.startswith("*")
            )
            if not any(ord(c) > 127 for c in line):
                continue
            strs = pat.findall(line)
            bad_strs = []
            for s in strs:
                content = s[1:-1]
                if any(ord(c) > 127 for c in content):
                    bad_strs.append(s if len(s) <= 140 else s[:137] + "...")
            special = (
                "CharPointer_UTF8" in line
                or ("juce_wchar" in line and "0x" in line)
                or "charToString" in line
            )
            if not bad_strs and not special:
                if is_comment:
                    continue
                # non-ascii outside strings on code line (e.g. identifiers)
                continue
            chars = sorted({f"{c} U+{ord(c):04X}" for c in line if ord(c) > 127})
            rel = os.path.relpath(path, root)
            results.append((rel, i, chars, bad_strs, is_comment, special, line.rstrip()[:220]))

print(f"Found {len(results)} lines with non-ASCII in strings/UI glyphs\n")
for rel, i, chars, bad, is_comment, special, line in results:
    kind = "comment+str" if is_comment else ("glyph" if special and not bad else "string")
    print(f"[{kind}] {rel}:{i}")
    print(f"  chars: {', '.join(chars)}")
    for b in bad:
        print(f"  str: {b}")
    if special and not bad:
        print(f"  line: {line}")
    print()
