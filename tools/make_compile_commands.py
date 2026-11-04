#!/usr/bin/env python3
"""Gives the editor's clangd the real ESP32 compile settings.

Writes compile_commands.json into zone_monitor and zone_cam. Only the editor
reads these. Run this again on a new computer or after updating the ESP32 core:

    python3 tools/make_compile_commands.py
"""

import json
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKETCHES = {
    "zone_monitor": "esp32:esp32:esp32s3",
    "zone_cam": "esp32:esp32:esp32cam",
}
SOURCE_SUFFIXES = {".c", ".cpp"}


def toolchain_headers(gcc: str, language: str) -> list[str]:
    """Asks the real compiler for its built-in include folders."""
    result = subprocess.run(
        [gcc, "-E", "-x", language, "-v", "/dev/null"],
        capture_output=True,
        text=True,
        check=True,
    )
    inside = False
    folders = []
    for line in result.stderr.splitlines():
        if line.startswith("#include <...>"):
            inside = True
        elif line.startswith("End of search list"):
            inside = False
        elif inside:
            folder = line.strip()
            # Skip gcc's own stdatomic.h and similar: clang needs its own.
            if "/lib/gcc/" in folder and "/c++/" not in folder and "xtensa-esp-elf/include" not in folder:
                continue
            folders.append(folder)
    return folders


def build_entries(sketch: str, fqbn: str, build_dir: Path) -> list[dict]:
    subprocess.run(
        [
            "arduino-cli", "compile", "--fqbn", fqbn,
            "--only-compilation-database",
            "--build-path", str(build_dir),
            str(ROOT / sketch),
        ],
        capture_output=True,
        check=True,
    )
    database = json.loads((build_dir / "compile_commands.json").read_text())
    entries = []
    for item in database:
        copied = Path(item["file"])
        try:
            relative = copied.relative_to(build_dir / "sketch")
        except ValueError:
            continue
        source = ROOT / sketch / relative
        if source.suffix not in SOURCE_SUFFIXES or not source.exists():
            continue
        arguments = item.get("arguments") or shlex.split(item["command"])
        gcc = arguments[0]
        language = "c" if source.suffix == ".c" else "c++"
        for folder in toolchain_headers(gcc, language):
            arguments = arguments + ["-isystem", folder]
        arguments = ["clang"] + arguments[1:]
        # Point at the real file, not the build folder's copy.
        arguments = [str(source) if a == str(copied) else a for a in arguments]
        entries.append(
            {"directory": str(ROOT / sketch), "file": str(source), "arguments": arguments}
        )
    return entries


def main() -> None:
    for sketch, fqbn in SKETCHES.items():
        with tempfile.TemporaryDirectory() as temp:
            entries = build_entries(sketch, fqbn, Path(temp).resolve())
        (ROOT / sketch / "compile_commands.json").write_text(json.dumps(entries, indent=1))
        print(f"{sketch}: {len(entries)} files")


if __name__ == "__main__":
    main()
