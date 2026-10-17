#!/usr/bin/env python3
"""Диагностика нативной и контейнерной сборки без установки пакетов."""
import os
import platform
import shutil
import subprocess
import sys

print(f'Система: {platform.system()} {platform.machine()}')
print(f'Python: {sys.version.split()[0]}')
for command in ['git', 'cmake', 'ninja', os.environ.get('CXX', 'clang++'), 'docker', 'brew', 'pg_config']:
    path = shutil.which(command)
    print(f'{command}: {path or "не найден"}')
if platform.system() == 'Darwin':
    result = subprocess.run(['xcode-select', '-p'], text=True, capture_output=True)
    print('Command Line Tools: ' + (result.stdout.strip() if result.returncode == 0 else 'нужен xcode-select --install'))
print('Для Docker нужен запущенный Docker Engine; для macOS см. docs/development.md.')
