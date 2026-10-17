#!/usr/bin/env bash
# Нативная сборка использует Command Line Tools; редактор может быть любым.
set -euo pipefail
xcode-select -p >/dev/null || { echo 'Сначала выполните xcode-select --install' >&2; exit 1; }
command -v brew >/dev/null || { echo 'Установите Homebrew: https://brew.sh' >&2; exit 1; }
brew install cmake ninja python@3.12 ccache boost c-ares cctz cryptopp fmt grpc \
    google-benchmark googletest jemalloc libev nghttp2 openssl@3 protobuf pugixml \
    yaml-cpp icu4c openldap clang-format postgresql@16 zlib zstd bzip2 pkgconf xxhash re2 abseil
printf '\nДобавьте в PATH: %s/opt/postgresql@16/bin и %s/opt/python@3.12/libexec/bin\n' "$(brew --prefix)" "$(brew --prefix)"
printf 'Затем: make doctor && make gen && make test-debug\n'
