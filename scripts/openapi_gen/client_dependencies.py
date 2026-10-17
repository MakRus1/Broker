"""Входы CMake для контрактов клиентов, без зависимостей от серверных targets."""
from pathlib import Path
import sys
from generate import UniqueLoader, specification_files, public_schema_dir
from clients import client_specs

if __name__ == '__main__':
    try:
        paths = set()
        for _, provider in client_specs(Path(sys.argv[1]), UniqueLoader):
            paths.update(specification_files(provider, public_only=True))
            # Изменение набора файлов также должно приводить к реконфигурации.
            paths.update(p for p in public_schema_dir(provider).rglob('*') if p.is_dir())
            paths.add(public_schema_dir(provider))
        print('\n'.join(str(p) for p in sorted(paths)))
    except (ValueError, OSError) as exc:
        sys.exit(str(exc))
