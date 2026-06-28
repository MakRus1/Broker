#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="${1:-}"
shift || true
EXTRA_FLAGS=("$@")

if [[ -z "${SERVICE_NAME}" ]]; then
  echo "Usage: $0 <service_name> [--postgresql] [--grpc] ..." >&2
  exit 1
fi

SERVICE_DIR="${ROOT}/services/${SERVICE_NAME}"
if [[ -e "${SERVICE_DIR}" ]]; then
  echo "Error: ${SERVICE_DIR} already exists." >&2
  exit 1
fi

USERVER_DIR="${ROOT}/third_party/userver"
if [[ ! -d "${USERVER_DIR}/scripts" ]]; then
  USERVER_DIR="/tmp/userver-setup"
  if [[ ! -d "${USERVER_DIR}/scripts" ]]; then
    git clone --depth 1 --branch v2.15 https://github.com/userver-framework/userver.git "${USERVER_DIR}"
  fi
fi

python3 "${USERVER_DIR}/scripts/userver-create-service.py" "${EXTRA_FLAGS[@]}" "${SERVICE_DIR}"

python3 - "${SERVICE_DIR}/CMakeLists.txt" <<'PY'
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()

text = re.sub(
    r"list\(APPEND CMAKE_MODULE_PATH.*?\ninclude\(DownloadUserver\)\n\n",
    "",
    text,
    flags=re.DOTALL,
)
text = re.sub(
    r"find_package\(\s*userver.*?\)\nif\(NOT userver_FOUND\).*?\)\n\n",
    "",
    text,
    flags=re.DOTALL,
)
text = re.sub(
    r"userver_setup_environment\(\)\n\n",
    "",
    text,
)
text = re.sub(
    r"PUBLIC userver::core",
    "PUBLIC userver::core broker_common",
    text,
    count=1,
)

path.write_text(text)
PY

rm -rf "${SERVICE_DIR}/cmake" "${SERVICE_DIR}/CMakePresets.json" "${SERVICE_DIR}/.devcontainer" "${SERVICE_DIR}/.github"
sed "s/@SERVICE_NAME@/${SERVICE_NAME}/g" "${ROOT}/scripts/service-Makefile.template" > "${SERVICE_DIR}/Makefile"

POSTGRES_FLAG=()
if [[ " ${EXTRA_FLAGS[*]} " == *" --postgresql "* ]]; then
  POSTGRES_FLAG=(--postgresql)
fi

python3 "${ROOT}/scripts/service_deps.py" add-service "${SERVICE_NAME}" "${POSTGRES_FLAG[@]}"

echo ""
echo "Сервис создан: services/${SERVICE_NAME}"
echo "Зарегистрирован в .github/service-deps.yaml и CMakeLists.txt"
if [[ " ${EXTRA_FLAGS[*]} " == *" --postgresql "* ]]; then
  echo "PostgreSQL контейнер добавлен в docker-compose.yml"
fi
echo ""
echo "Сборка и тесты:"
echo "  make test-debug SERVICE=${SERVICE_NAME}"
