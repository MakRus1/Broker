#!/usr/bin/env python3
"""Проверка версий и имен SQL-миграций, порядок совпадает с testsuite."""
from pathlib import Path
import re
import sys


def check(root):
    for directory in [root / 'services', root / 'scripts/templates/service']:
        for path in directory.rglob('postgresql/schemas'):
            for database in path.iterdir():
                if not database.is_dir() or not re.fullmatch(r'[a-z][a-z0-9_]*', database.name):
                    raise ValueError(f'{database}: ожидается каталог базы данных')
                versions = set()
                for migration in database.iterdir():
                    match = re.fullmatch(r'V([0-9]{4})__[a-z][a-z0-9]*(?:_[a-z0-9]+)*\.sql', migration.name)
                    if not migration.is_file() or not match or int(match[1]) == 0:
                        raise ValueError(f'{migration}: ожидается V0001__migration_name.sql')
                    if match[1] in versions:
                        raise ValueError(f'{migration}: повтор версии миграции')
                    versions.add(match[1])


if __name__ == '__main__':
    try:
        check(Path(__file__).resolve().parents[1])
    except ValueError as exc:
        sys.exit(str(exc))
