#!/usr/bin/env bash
# Окружение локальной сборки хранится в volumes, исходники монтируются только для чтения.
set -euo pipefail
chown 1001:1001 /workspaces/broker/build-local /workspaces/broker/.venv-gen /logs /cache
exec runuser -u builder -- bash -euc '
  scripts/openapi_gen/ensure_venv.sh >/dev/null
  selected_services="$*"
  shared_contracts=OFF
  if (( $# == 0 )); then
    selected_services=all
    shared_contracts=ON
    read -r -a services <<< "$(.venv-gen/bin/python scripts/service_catalog.py --plain)"
    set -- "${services[@]}" contract_service
  fi
  .venv-gen/bin/cmake -S . -B build-local -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DBUILD_TESTING=ON -DPython3_EXECUTABLE="$PWD/.venv-gen/bin/python" -DCMAKE_MAKE_PROGRAM="$PWD/.venv-gen/bin/ninja" -DBROKER_SERVICES="$selected_services" -DBROKER_SHARED_CONTRACTS="$shared_contracts"
  .venv-gen/bin/cmake --build build-local -j "${NPROCS:-4}" --target "$@"
' -- "$@"
