#!/usr/bin/env bash
# Определяет сервисы для CI по diff от BASE (commit или ref).
# Вывод: "all" или список имён через пробел (например: "broker orders").
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS_FILE="${ROOT}/.github/service-deps.yaml"
BASE="${1:-origin/main}"

if ! git -C "$ROOT" rev-parse --verify "${BASE}^{commit}" >/dev/null 2>&1; then
  echo "all"
  exit 0
fi

if git -C "$ROOT" diff --quiet "${BASE}" HEAD; then
  exit 0
fi

is_shared_path() {
  local path="$1"
  while IFS= read -r prefix; do
    [[ -z "${prefix}" || "${prefix}" == \#* ]] && continue
    [[ "${path}" == "${prefix}" || "${path}" == "${prefix}"* ]] && return 0
  done < <(awk '/^shared_paths:/{flag=1;next} /^[^ ]/{flag=0} flag && /^  - /{print substr($0,5)}' "${DEPS_FILE}")
  return 1
}

lib_dependents() {
  local lib="$1"
  awk -v lib="${lib}" '
    $0 ~ "^  " lib ":$" { in_lib=1; next }
    in_lib && /^    services:$/ { in_list=1; next }
    in_lib && in_list && /^      - / { print substr($0, 9); next }
    in_lib && /^  [^ ]/ { exit }
    in_lib && /^[^ ]/ { exit }
  ' "${DEPS_FILE}"
}

add_unique() {
  local item="$1"
  local existing
  for existing in ${AFFECTED_LIST:-}; do
    [[ "${existing}" == "${item}" ]] && return 0
  done
  AFFECTED_LIST="${AFFECTED_LIST:+${AFFECTED_LIST} }${item}"
}

AFFECTED_LIST=""

while IFS= read -r path; do
  [ -z "${path}" ] && continue

  if is_shared_path "${path}"; then
    echo "all"
    exit 0
  fi

  case "${path}" in
    libs/*)
      lib="${path#libs/}"
      lib="${lib%%/*}"
      while IFS= read -r svc; do
        [[ -n "${svc}" ]] && add_unique "${svc}"
      done < <(lib_dependents "${lib}")
      ;;
    services/*)
      svc="${path#services/}"
      svc="${svc%%/*}"
      add_unique "${svc}"
      ;;
  esac
done < <(git -C "$ROOT" diff --name-only "${BASE}" HEAD)

if [ -z "${AFFECTED_LIST}" ]; then
  exit 0
fi

echo "${AFFECTED_LIST}" | tr ' ' '\n' | sort | tr '\n' ' ' | xargs
