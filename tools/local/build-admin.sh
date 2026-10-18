#!/usr/bin/env bash
# Исходники доступны только для чтения; кеш и результат сборки живут в volumes.
set -euo pipefail
cache=/node-cache
mkdir -p "$cache/work" "$cache/tooling" /admin-ui
if [[ ! -x "$cache/tooling/node_modules/.bin/pnpm" ]]; then
    npm install --prefix "$cache/tooling" --no-audit --no-fund pnpm@11.25.0
fi
tar --exclude=node_modules --exclude=dist --exclude=coverage -C frontend/admin -cf - . | tar -C "$cache/work" -xf -
manager="$cache/tooling/node_modules/.bin/pnpm"
"$manager" --dir "$cache/work" install --frozen-lockfile --store-dir "$cache/store"
"$manager" --dir "$cache/work" build
cp -R "$cache/work/dist/." /admin-ui/
