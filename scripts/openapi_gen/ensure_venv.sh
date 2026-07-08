#!/usr/bin/env bash
# Creates .venv-gen with PyYAML and prints the venv python path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV="${ROOT}/.venv-gen"
PYTHON="${VENV}/bin/python"
REQUIREMENTS="${ROOT}/requirements.txt"

_find_python3() {
    local candidate
    for candidate in \
        "${PYTHON3:-}" \
        python3 \
        /opt/homebrew/bin/python3 \
        /usr/local/bin/python3 \
        /usr/bin/python3
    do
        [[ -n "${candidate}" ]] || continue
        if command -v "${candidate}" >/dev/null 2>&1 \
            && "${candidate}" -c 'import sys; assert sys.version_info >= (3, 9)' 2>/dev/null
        then
            command -v "${candidate}"
            return 0
        fi
    done
    return 1
}

_python_works() {
    [[ -x "$1" ]] && "$1" -c 'import yaml' 2>/dev/null
}

_create_venv() {
    local base_python="$1"
    rm -rf "${VENV}"
    "${base_python}" -m venv "${VENV}"
    "${PYTHON}" -m pip install -q --upgrade pip
    "${PYTHON}" -m pip install -q -r "${REQUIREMENTS}"
}

if ! _python_works "${PYTHON}"; then
    base_python="$(_find_python3)" || {
        echo 'python3 >= 3.9 not found (install Homebrew python or Xcode CLT)' >&2
        exit 1
    }
    _create_venv "${base_python}"
fi

if ! _python_works "${PYTHON}"; then
    echo "Failed to prepare codegen venv with PyYAML (${PYTHON})" >&2
    exit 1
fi

printf '%s\n' "${PYTHON}"
