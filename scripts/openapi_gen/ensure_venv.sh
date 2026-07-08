#!/usr/bin/env bash
# Creates .venv-gen with PyYAML and prints the venv python path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV="${ROOT}/.venv-gen"
PYTHON="${VENV}/bin/python"

if [[ ! -x "${PYTHON}" ]]; then
    python3 -m venv "${VENV}"
fi

if ! "${PYTHON}" -c 'import yaml' 2>/dev/null; then
    "${VENV}/bin/pip" install -q -r "${ROOT}/requirements.txt"
fi

printf '%s\n' "${PYTHON}"
