"""Изолированные входы генератора без посторонних бизнесовых сервисов и сборок."""

from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def copy_generator_repository(destination):
    # Broker — небольшой поставщик контракта; Configs проверяет обновление registry lock.
    # Копии файлов независимы: тесты меняют manifests, контракты и библиотеки.
    directories = [
        'cmake',
        'libs',
        'scripts/openapi_gen',
        'scripts/templates/service',
        'services/broker',
        'services/configs',
        'schemas/services/broker',
        'schemas/services/configs',
        'schemas/configs/shipment',
        'schemas/config_contexts/shipment',
        'schemas/permissions',
    ]
    ignore = shutil.ignore_patterns('__pycache__', '*.pyc', '.DS_Store')
    for directory in directories:
        shutil.copytree(ROOT / directory, destination / directory, ignore=ignore)
    for filename in [
        '.clang-format',
        'CMakeLists.txt',
        '.github/service-deps.yaml',
        'docker-compose.yml',
    ]:
        target = destination / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / filename, target)
