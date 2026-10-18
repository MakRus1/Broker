#!/usr/bin/env bash
# Окружение локальной сборки хранится в volumes, исходники монтируются только для чтения.
set -euo pipefail
chown 1001:1001 /workspaces/broker/build-local /workspaces/broker/.venv-gen /logs /cache
exec runuser -u builder -- bash -euc '
  scripts/openapi_gen/ensure_venv.sh >/dev/null
  .venv-gen/bin/cmake --fresh -S . -B build-local -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DBUILD_TESTING=ON -DPython3_EXECUTABLE="$PWD/.venv-gen/bin/python" -DCMAKE_MAKE_PROGRAM="$PWD/.venv-gen/bin/ninja"
  .venv-gen/bin/cmake --build build-local -j "${NPROCS:-4}" --target broker configs contract_service admin_bff
'
