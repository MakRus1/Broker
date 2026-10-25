#!/usr/bin/env python3
"""Запуск тестов инструментов, затронутых изменёнными файлами."""

import argparse
import fnmatch
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

# Общая основа может менять поведение любого инструмента: проверяем весь набор.
FULL = (
    '.github/workflows/ci.yml',
    'scripts/ci-tools.py',
    'scripts/ci-tests.py',
    'scripts/affected-services*',
    '.github/service-deps.yaml',
    'requirements.txt',
    'tools/versions.env',
    'tools/python-constraints.txt',
    'tests/support/*',
)
GENERATOR = (
    'scripts/openapi_gen/*',
    'scripts/templates/*',
    'scripts/create_service.py',
    'scripts/smoke-services.py',
    'scripts/service_catalog.py',
    'cmake/*',
    'CMakeLists.txt',
    'CMakePresets.json',
    'Makefile',
    'tests/fixtures/*',
)
GENERATOR_TESTS = {
    'test_auth_codegen',
    'test_clients',
    'test_cmake',
    'test_selected_services',
    'test_codegen',
    'test_configs',
    'test_dashboards',
    'test_documents',
    'test_export_clients',
    'test_http_logging',
    'test_models',
    'test_pg_types',
    'test_runtime',
    'test_service_accounts',
    'test_service_catalog',
    'test_services',
    'test_versions',
}
INFRASTRUCTURE = (
    'scripts/staging*',
    'scripts/deployment.py',
    'scripts/migrations.py',
    'scripts/organization_identity.py',
    'scripts/permission_catalog.py',
    'scripts/local-env.py',
    'scripts/ci-local.py',
    'tools/deploy/*',
    'tools/local/*',
    'tools/observability/*',
    'platform.yaml',
    'docker-compose*',
    'services/*/postgresql/schemas/*',
    'services/*/configs/*',
)
INFRA_TESTS = {
    'test_deploy_cluster',
    'test_deployment',
    'test_local_seed',
    'test_ci_local',
    'test_local_clients',
    'test_local_workflow',
    'test_log_ingestion',
    'test_migrations',
    'test_organization_identity',
    'test_permission_catalog',
    'test_staging_backup',
    'test_staging_connect',
    'test_staging_deploy',
    'test_staging_lan',
    'test_staging_monitoring',
    'test_staging_network',
    'test_staging_rollback',
}


def matches(path, patterns):
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def select(paths, available):
    if paths is None:
        return {name: 'Нет базовой ревизии — полный набор' for name in available}
    selected = {}
    for path in paths:
        if path.endswith('.md'):
            continue
        if matches(path, FULL):
            return {name: 'Общая основа: ' + path for name in available}
        groups = set()
        # Контракты и manifests — входы генератора, а не изменение его реализации.
        # Их валидность проверяют check-gen-all и сборки поставщика/клиентов.
        if matches(path, ('services/*/service.yaml',)):
            groups |= {'test_local_clients', 'test_permission_catalog', 'test_deployment'}
        if path.startswith(('schemas/configs/', 'schemas/config_contexts/')):
            groups.add('test_configs')
        if path.startswith('schemas/permissions/'):
            groups.add('test_permission_catalog')
        if matches(path, GENERATOR):
            groups |= GENERATOR_TESTS
        if matches(path, INFRASTRUCTURE):
            groups |= INFRA_TESTS
        if path.startswith('tests/test_') and path.endswith('.py'):
            groups.add(Path(path).stem)
        if (
            path.startswith('tools/container-images')
            or path.startswith('tools/docker/')
            or path == 'tools/Dockerfile'
        ):
            groups |= {'test_container_images', 'test_versions'}
        if (
            path.startswith('scripts/cpp_style')
            or path == 'scripts/check-format.py'
            or path == '.clang-format'
        ):
            groups.add('test_cpp_style')
        if path == 'scripts/check-yaml.py':
            groups.add('test_yaml_style')
        # Новый или неописанный общий инструмент не должен молча остаться без тестов.
        if not groups and path.startswith(('scripts/', 'tools/', 'tests/')):
            return {name: 'Неописанная зависимость инструментов: ' + path for name in available}
        for name in sorted(groups & set(available)):
            selected.setdefault(name, path)
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='missing-base')
    parser.add_argument('--full', action='store_true')
    parser.add_argument('--plan-only', action='store_true')
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('ci_tests', ROOT / 'scripts/ci-tests.py')
    ci = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ci)
    available = sorted(path.stem for path in (ROOT / 'tests').glob('test_*.py'))
    chosen = select(None if args.full else ci.changed_paths(args.base), available)
    summary = '### Тесты инструментов\n\n'
    summary += (
        '\n'.join(f'- `{name}`: {reason}' for name, reason in sorted(chosen.items()))
        if chosen
        else 'Не затронуты — тесты генератора и инфраструктуры пропущены.'
    )
    print(summary, flush=True)
    if target := os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(target, 'a') as stream:
            stream.write(summary + '\n\n')
    if args.plan_only or not chosen:
        return
    sys.path.insert(0, str(ROOT / 'tests'))
    suite = unittest.defaultTestLoader.loadTestsFromNames(sorted(chosen))
    if not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful():
        raise SystemExit(1)


if __name__ == '__main__':
    main()
