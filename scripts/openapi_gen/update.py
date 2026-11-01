#!/usr/bin/env python3
"""Обновление отслеживаемой генерации одной командой, включая зависимые сервисы."""
import argparse
from configs import cached_http_catalog
from pathlib import Path
import yaml

from export_clients import export
from generate import run, StaleGeneration


def refresh(service, invalidate):
    # Записываем обвязку до экспорта: service.yaml может добавлять ручки.
    run(service)
    if (service / 'client-api.yaml').is_file():
        export(service)
        invalidate()
        # HTTP-настройка должна учитывать уже обновлённый публичный API.
        run(service)


def update(service):
    with cached_http_catalog() as invalidate:
        return update_services(service, invalidate)


def update_services(service, invalidate):
    root = service.parent.parent
    if service.parent.name != 'services' or not (service / 'service.yaml').is_file():
        raise ValueError('Ожидается каталог services/<имя> с service.yaml')
    refresh(service, invalidate)
    changed = [service.name]
    candidates = sorted(path for path in (root / 'services').iterdir()
                        if (path / 'service.yaml').is_file() and path != service)
    fixture = root / 'tests/fixtures/contract_service'
    if fixture.is_dir():
        candidates.append(fixture)
    # Проверка читает только входы/отслеживаемые файлы, не рендерит C++-клиенты.
    # Незатронутые сервисы не записываются. Проверка остаётся строго read-only в CI.
    for _ in range(len(candidates) + 2):
        refreshed = False
        for candidate in [service, *candidates]:
            try:
                run(candidate, check=True)
            except StaleGeneration:
                refresh(candidate, invalidate)
                refreshed = True
                if candidate.name not in changed:
                    changed.append(candidate.name)
        if not refreshed:
            return changed
    raise ValueError('Генерация не стабилизировалась: проверьте зависимости схем')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service-dir', type=Path, required=True)
    args = parser.parse_args()
    try:
        changed = update(args.service_dir.resolve())
    except (ValueError, OSError, KeyError, TypeError, yaml.YAMLError) as error:
        parser.exit(1, f'{args.service_dir}: {error}\n')
    print('Обновлена генерация: ' + ', '.join(changed))


if __name__ == '__main__':
    main()
