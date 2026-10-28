#!/usr/bin/env bash
# Исходники доступны только для чтения; кеш и результат сборки живут в volumes.
set -euo pipefail
app=${1:-admin}
output=${2:-/admin-ui}
cache=/node-cache
mkdir -p "$cache/work" "$cache/tooling" "$output"
if [[ ! -x "$cache/tooling/node_modules/.bin/pnpm" ]]; then
    npm install --prefix "$cache/tooling" --no-audit --no-fund pnpm@11.25.0
fi
# Обновляем копию исходников целиком: tar поверх старого дерева оставлял удалённые файлы.
# Установленные зависимости и pnpm store сохраняются между сборками.
find "$cache/work" -mindepth 1 -maxdepth 1 ! -name node_modules -exec rm -rf -- {} +
tar --exclude=node_modules --exclude=dist --exclude=coverage -C "frontend/$app" -cf - . | tar -C "$cache/work" -xf -
manager="$cache/tooling/node_modules/.bin/pnpm"
"$manager" --dir "$cache/work" install --frozen-lockfile --store-dir "$cache/store"
"$manager" --dir "$cache/work" build
cp -R "$cache/work/dist/." "$output"/
