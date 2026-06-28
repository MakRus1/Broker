#!/usr/bin/env bash
set -euo pipefail

DEPS_FILE="https://raw.githubusercontent.com/userver-framework/userver/refs/heads/develop/scripts/docs/en/deps/macos.md"

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew не найден. Установите его с https://brew.sh" >&2
  exit 1
fi

echo "Устанавливаю зависимости userver для macOS..."
brew install python@3.13 ninja $(curl -fsSL "${DEPS_FILE}")

echo ""
echo "Добавьте в ~/.zshrc (если ещё не добавлено):"
echo '  export PATH="/opt/homebrew/opt/python@3.13/bin:/opt/homebrew/bin:$PATH"'
echo ""
echo "Готово. Далее:"
echo "  pip3 install -r requirements.txt"
echo "  make cmake-debug && make build-debug"
