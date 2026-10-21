#!/usr/bin/env python3
"""Запрет YAML-якорей и ссылок в отслеживаемых файлах репозитория."""
from pathlib import Path
import subprocess
import sys

import yaml


def check_file(path):
    for token in yaml.scan(path.read_text(encoding='utf-8')):
        if isinstance(token, (yaml.tokens.AnchorToken, yaml.tokens.AliasToken)):
            line = token.start_mark.line + 1
            raise ValueError(f'{path}:{line}: YAML-якоря и ссылки запрещены; повторите определение явно')


def check_repository(root):
    paths = subprocess.check_output(
        ['git', 'ls-files', '-z', '--', '*.yaml', '*.yml'], cwd=root
    ).decode().split('\0')
    for relative in filter(None, paths):
        path = root / relative
        if path.is_file():
            check_file(path)


if __name__ == '__main__':
    try:
        check_repository(Path(__file__).resolve().parents[1])
    except (ValueError, OSError, yaml.YAMLError, subprocess.CalledProcessError) as exc:
        sys.exit(str(exc))
