#!/usr/bin/env python3
"""План выборочных проверок и отчеты CTest/pytest для GitHub Actions."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent


def service_names(values):
    names = list(dict.fromkeys(values))
    for name in names:
        if not re.fullmatch(r'[a-z][a-z0-9_]*', name):
            raise ValueError(f'Некорректное имя сервиса: {name!r}')
    return names


def append(path, content):
    if path:
        with Path(path).open('a') as stream:
            stream.write(content)


def changed_paths(base):
    exists = subprocess.run(
        ['git', '-C', str(ROOT), 'rev-parse', '--verify', base + '^{commit}'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if exists.returncode:
        return None
    return (
        subprocess.run(
            ['git', '-C', str(ROOT), 'diff', '--name-only', '-z', base, 'HEAD'],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        .stdout.strip('\0')
        .split('\0')
    )


def select_checks(paths, services):
    scaffold_paths = (
        'scripts/templates/',
        'scripts/openapi_gen/',
        'scripts/create_service.py',
        'scripts/smoke-services.py',
        'scripts/ci-tests.py',
        'cmake/',
        'libs/common/',
        'CMakeLists.txt',
        'CMakePresets.json',
        'Makefile',
        'requirements.txt',
        'tools/versions.env',
        'tools/python-constraints.txt',
        'tools/container-images.json',
        'tools/Dockerfile',
        'tools/docker/',
        '.github/workflows/environment.yml',
        '.devcontainer/',
        '.github/workflows/ci.yml',
        'tests/test_services.py',
    )
    scaffolds = paths is None or any(
        path == prefix.rstrip('/')
        or path.startswith(prefix if prefix.endswith('/') else prefix + '/')
        for path in (paths or [])
        for prefix in scaffold_paths
    )
    frontends = [
        name
        for name, bff in [('admin', 'admin_bff'), ('web', 'web_bff')]
        if paths is None
        or bff in services
        or any(path.startswith((f'frontend/{name}/', 'frontend/shared/')) for path in paths)
    ]
    smoke = (
        paths is None
        or bool({'shipments', 'transportations'} & set(services))
        or any(path.startswith('tests/integration/') for path in paths)
    )
    build_services = sorted(set(services) | ({'shipments', 'transportations'} if smoke else set()))
    shared_contracts = (
        scaffolds
        or paths is None
        or any(
            path.startswith(('tests/fixtures/contract_service/', 'libs/common/', 'libs/configs/'))
            for path in (paths or [])
        )
    )
    return {
        'shared_contracts': shared_contracts,
        'scaffolds': scaffolds,
        'smoke': smoke,
        'frontends': frontends,
        'build_services': build_services,
    }


def build(args):
    services = service_names(args.services)
    inventory = json.loads(
        subprocess.check_output(
            [args.ctest, '--test-dir', str(args.build_dir), '--show-only=json-v1'], text=True
        )
    )
    tests = {test['name'] for test in inventory['tests']}
    targets = list(services)
    if args.shared_contracts:
        targets.append('contract_service')
    targets += [name + '_unittest' for name in services if name + '_unittest' in tests]
    if args.shared_contracts and 'client_contract_unittest' in tests:
        targets.append('client_contract_unittest')
    if not targets:
        raise ValueError('Пустой список целей: неявная сборка всего проекта запрещена')
    command = [
        args.cmake,
        '--build',
        str(args.build_dir),
        '-j',
        str(args.jobs),
        '--target',
        *targets,
    ]
    print('Цели сборки: ' + ', '.join(targets), flush=True)
    subprocess.run(command, check=True)


def plan(args):
    result = subprocess.run(
        [sys.executable, str(ROOT / 'scripts/affected-services.py'), args.base, '--json'],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.strip()
    impact = json.loads(result)
    available = sorted(path.name for path in (ROOT / 'services').iterdir() if path.is_dir())
    selected = service_names(impact['services'])
    if not set(selected) <= set(available):
        raise ValueError('Определение зависимостей вернуло неизвестные сервисы')
    checks = select_checks(changed_paths(args.base), selected)
    build_required = (
        bool(checks['build_services']) or checks['shared_contracts'] or checks['scaffolds']
    )
    message = ', '.join(f'`{name}`' for name in selected) or (
        'нет — проверяются общие контракты'
        if build_required
        else 'нет — сборка и CTest пропускаются'
    )
    text = f'### План проверок сервисов\n\nВыбраны: {message}.\n\n'
    for service in selected:
        text += f'- `{service}`: ' + '; '.join(impact['reasons'][service]) + '\n'
    text += '\n'
    if build_required:
        text += 'Окружения: Debug/Release/ASan+UBSan на amd64, Debug на arm64.\n'
    if extra := set(checks['build_services']) - set(selected):
        text += 'Дополнительные executable для smoke: ' + ', '.join(sorted(extra)) + '.\n'
    text += f'\nПроверка шаблона: {"да" if checks["scaffolds"] else "нет"}. Межсервисный smoke: {"да" if checks["smoke"] else "нет"}.\n'
    text += f'Общие контрактные тесты: {"да" if checks["shared_contracts"] else "нет"}.\n'
    text += 'Фронтенды: ' + (', '.join(checks['frontends']) or 'нет') + '.\n'
    print(text)
    append(
        args.github_output,
        f'build_required={str(build_required).lower()}\n'
        f'shared_contracts={str(checks["shared_contracts"]).lower()}\n'
        f'scaffolds={str(checks["scaffolds"]).lower()}\n'
        f'smoke={str(checks["smoke"]).lower()}\n'
        f'frontends={json.dumps(checks["frontends"])}\n'
        f'build_services={" ".join(checks["build_services"])}\n',
    )
    append(args.github_output, f'services={" ".join(selected)}\n')
    append(args.summary, text)


def counts(path):
    cases = ET.parse(path).getroot().findall('.//testcase')
    if not cases:
        raise ValueError(f'В отчете нет тестов: {path}')
    failed = sum(
        case.find('failure') is not None or case.find('error') is not None for case in cases
    )
    skipped = sum(case.find('skipped') is not None for case in cases)
    return {'passed': len(cases) - failed - skipped, 'failed': failed, 'skipped': skipped}


def run(args):
    services = service_names(args.services)
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'results.json').write_text('[]\n')
    inventory = json.loads(
        subprocess.check_output(
            [args.ctest, '--test-dir', str(args.build_dir), '--show-only=json-v1'],
            text=True,
        )
    )
    names = {test['name'] for test in inventory['tests']}
    groups = [
        (service, f'^({re.escape(service)}_unittest|testsuite-{re.escape(service)})$')
        for service in services
    ]
    if args.shared_contracts:
        groups.append(
            ('shared-contracts', '^(testsuite-contract_service|client_contract_unittest)$')
        )
    if not groups:
        raise ValueError('Пустой список тестов')
    # Общие проверки не должны скрывать отсутствие тестов выбранного сервиса.
    for name, pattern in groups:
        if not any(re.fullmatch(pattern, test) for test in names):
            raise ValueError(f'Не найдены тесты группы {name}; проверка не может быть зеленой')
    results = []
    for name, pattern in groups:
        for report in (output / f'{name}.xml', output / f'pytest-{name}.xml'):
            report.unlink(missing_ok=True)
        command = [
            args.ctest,
            '--test-dir',
            str(args.build_dir),
            '--verbose',
            '--output-on-failure',
            '--no-tests=error',
            '-R',
            pattern,
            '--output-junit',
            str(output / f'{name}.xml'),
        ]
        print(f'::group::Тесты: {name}', flush=True)
        print(shlex.join(command), flush=True)
        environment = os.environ.copy()
        pytest_options = shlex.split(environment.get('PYTEST_ADDOPTS', ''))
        pytest_options.append(f'--junitxml={output / f"pytest-{name}.xml"}')
        environment['PYTEST_ADDOPTS'] = shlex.join(pytest_options)
        with (output / f'{name}.log').open('w') as log:
            with subprocess.Popen(
                command,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            ) as process:
                for line in process.stdout:
                    print(line, end='', flush=True)
                    log.write(line)
                code = process.wait()
        item = {'name': name, 'status': 'passed' if code == 0 else 'failed'}
        try:
            item['ctest'] = counts(output / f'{name}.xml')
            pytest_report = output / f'pytest-{name}.xml'
            if pytest_report.exists():
                item['pytest'] = counts(pytest_report)
            if any(item[key]['failed'] for key in ('ctest', 'pytest') if key in item):
                item['status'] = 'failed'
        except (OSError, ValueError, ET.ParseError) as error:
            item['status'] = 'failed'
            item['error'] = str(error)
        results.append(item)
        (output / 'results.json').write_text(
            json.dumps(results, ensure_ascii=False, indent=2) + '\n'
        )
        print(f'Результат {name}: {item["status"]}', flush=True)
        print('::endgroup::', flush=True)
    return int(any(item['status'] != 'passed' for item in results))


def summary(args):
    path = args.output_dir / 'results.json'
    results = {item['name']: item for item in json.loads(path.read_text())} if path.exists() else {}
    lines = [
        f'### Тесты сервисов · {args.preset} · {args.arch}',
        '',
        f'Сборка: **{args.build_result}**. Шаг тестов: **{args.test_result}**.',
        '',
        '| Сервис / группа | Результат | CTest: успешно / ошибки / пропуски | '
        'Pytest: успешно / ошибки / пропуски |',
        '| --- | --- | --- | --- |',
    ]
    groups = service_names(args.services) + (['shared-contracts'] if args.shared_contracts else [])
    for name in groups:
        item = results.get(name, {})

        def cell(key):
            value = item.get(key)
            return f'{value["passed"]} / {value["failed"]} / {value["skipped"]}' if value else '—'

        status = {'passed': '✅ Пройдены', 'failed': '❌ Ошибка'}.get(
            item.get('status'), '⏭ Не запущены'
        )
        lines.append(f'| `{name}` | {status} | {cell("ctest")} | {cell("pytest")} |')
    lines += [
        '',
        'CTest считает наборы (unit executable / testsuite); Pytest — отдельные '
        'функциональные тесты внутри testsuite. Эти столбцы не суммируются.',
        '',
        'Подробности: шаг «Тесты затронутых сервисов», блок с именем сервиса. '
        'Логи и JUnit: artifact `test-results-' + args.preset + '-' + args.arch + '`.',
        '',
    ]
    text = '\n'.join(lines)
    print(text)
    append(args.summary, text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    planning = commands.add_parser('plan')
    planning.add_argument('--base', default='missing-base')
    planning.add_argument('--github-output', default=os.environ.get('GITHUB_OUTPUT'))
    planning.add_argument('--summary', default=os.environ.get('GITHUB_STEP_SUMMARY'))
    building = commands.add_parser('build')
    building.add_argument('--build-dir', type=Path, required=True)
    building.add_argument('--services', nargs='*', default=[])
    building.add_argument('--cmake', default=str(ROOT / '.venv-gen/bin/cmake'))
    building.add_argument('--ctest', default=str(ROOT / '.venv-gen/bin/ctest'))
    building.add_argument('--jobs', type=int, default=4)
    running = commands.add_parser('run')
    running.add_argument('--build-dir', type=Path, required=True)
    running.add_argument('--ctest', default=str(ROOT / '.venv-gen/bin/ctest'))
    for command in (running, commands.add_parser('summary')):
        command.add_argument('--services', nargs='*', default=[])
        command.add_argument('--output-dir', type=Path, required=True)
    for command in (building, running, commands.choices['summary']):
        command.add_argument(
            '--shared-contracts',
            action=argparse.BooleanOptionalAction,
            default=os.environ.get('CI_SHARED_CONTRACTS', 'true') != 'false',
        )
    reporting = commands.choices['summary']
    reporting.add_argument('--preset', required=True)
    reporting.add_argument('--arch', required=True)
    reporting.add_argument('--build-result', required=True)
    reporting.add_argument('--test-result', required=True)
    reporting.add_argument('--summary', default=os.environ.get('GITHUB_STEP_SUMMARY'))
    args = parser.parse_args()
    return {'plan': plan, 'build': build, 'run': run, 'summary': summary}[args.command](args)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.CalledProcessError) as error:
        sys.exit(f'Ошибка проверок CI: {error}')
