#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "${PYTHON3:-python3}" "$ROOT/scripts/affected-services.py" "$@"
