#!/usr/bin/env bash
# Зависимости генератора устанавливаются в отдельное окружение с фиксированными версиями.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV="$ROOT/.venv-gen"
BASE="${PYTHON3:-$(command -v python3.12 || command -v python3)}"
"$BASE" -c 'import sys; assert sys.version_info >= (3, 10), "Нужен Python >= 3.10"'
# При смене базового Python пересоздаем только наше служебное окружение.
if [[ -x "$VENV/bin/python" ]]; then
    ACTUAL_BASE="$("$VENV/bin/python" -c 'import sys; print(sys.base_prefix)')"
    EXPECTED_BASE="$("$BASE" -c 'import sys; print(sys.base_prefix)')"
    if [[ "$ACTUAL_BASE" != "$EXPECTED_BASE" ]]; then
        rm -rf "$VENV"
    fi
fi
if [[ ! -x "$VENV/bin/python" ]]; then
    "$BASE" -m venv "$VENV"
fi
EXPECTED="$("$BASE" -c 'import hashlib, pathlib; print(hashlib.sha256(pathlib.Path(__import__("sys").argv[1]).read_bytes()).hexdigest())' "$ROOT/requirements.txt")"
if [[ ! -f "$VENV/.requirements.sha256" ]] || [[ "$(cat "$VENV/.requirements.sha256")" != "$EXPECTED" ]]; then
    "$VENV/bin/python" -m pip install --disable-pip-version-check -r "$ROOT/requirements.txt" >&2
    printf '%s' "$EXPECTED" > "$VENV/.requirements.sha256"
fi
printf '%s\n' "$VENV/bin/python"
