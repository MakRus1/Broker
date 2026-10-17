#!/usr/bin/env bash
# Совместимый вход для существующих скриптов.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON="$("$ROOT/scripts/openapi_gen/ensure_venv.sh")"
exec "$PYTHON" "$ROOT/scripts/create_service.py" "$@"
