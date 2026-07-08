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

python3 - "${SERVICE_DIR}/CMakeLists.txt" "${SERVICE_DIR}/src/main.cpp" "${SERVICE_NAME}" <<'PY'
import pathlib
import re
import sys

cmake_path = pathlib.Path(sys.argv[1])
main_path = pathlib.Path(sys.argv[2])
service_name = sys.argv[3]

text = cmake_path.read_text()
openapi_block = '''
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_CURRENT_SOURCE_DIR}")
'''

if 'broker_openapi_generate' not in text:
    text = text.replace('project(', openapi_block + '\nproject(', 1)

if '${OPENAPI_GEN_SRCS}' not in text:
    text = re.sub(
        r'(add_library\(\s*\n\s*\$\{PROJECT_NAME\}_objs OBJECT\n(?:.*\n)*?)(\))',
        lambda m: m.group(1) + '    ${OPENAPI_GEN_SRCS}\n' + m.group(2),
        text,
        count=1,
    )

if 'broker_openapi_apply' not in text:
    text = re.sub(
        r'(target_link_libraries\(\$\{PROJECT_NAME\} PRIVATE \$\{PROJECT_NAME\}_objs\)\n)',
        r'''\1if(OPENAPI_VIEW_SRCS)
    target_sources(${PROJECT_NAME} PRIVATE ${OPENAPI_VIEW_SRCS})
    broker_openapi_apply(${PROJECT_NAME})
endif()
''',
        text,
        count=1,
    )

cmake_path.write_text(text)

if main_path.exists():
    main = main_path.read_text()
    if 'openapi/handlers.hpp' not in main:
        main = main.replace(
            '#include <userver/utils/daemon_run.hpp>',
            '#include <userver/utils/daemon_run.hpp>\n\n#include <openapi/handlers.hpp>',
        )
        main = main.replace(
            'return userver::utils::DaemonMain(argc, argv, component_list);',
            f'    component_list = {service_name}::openapi::AppendGeneratedHandlers(component_list);\n\n    return userver::utils::DaemonMain(argc, argv, component_list);',
        )
        main_path.write_text(main)
PY

rm -rf "${SERVICE_DIR}/cmake" "${SERVICE_DIR}/.devcontainer" "${SERVICE_DIR}/.github"
mkdir -p "${SERVICE_DIR}/docs/api"
cp "${ROOT}/scripts/openapi_gen/docs-api-README.md" "${SERVICE_DIR}/docs/api/README.md"
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
