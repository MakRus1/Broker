"""Локальный цикл не собирает/перезапускает посторонние сервисы."""

import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('local_workflow', ROOT / 'scripts/local-env.py')
local = importlib.util.module_from_spec(spec)
spec.loader.exec_module(local)
spec = importlib.util.spec_from_file_location('wait_ready', ROOT / 'tools/local/wait_ready.py')
ready = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ready)


class LocalUpdateTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.state = Path(self.temp.name)
        self.patches = [
            mock.patch.object(local, 'STATE', self.state),
            mock.patch.object(local, 'prepare'),
            mock.patch.object(local, 'discover_services', return_value=['shipments', 'web_bff']),
        ]
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)
        (self.state / 'compose.yaml').write_text(
            yaml.safe_dump(
                {
                    'services': {
                        'shipments': {},
                        'web_bff': {},
                        'prepare': {},
                        'migrate-shipments': {},
                    }
                }
            )
        )
        config = yaml.safe_dump(
            {'components_manager': {'components': {'server': {'listener-monitor': {'port': 8102}}}}}
        )
        for service in ['shipments', 'web_bff']:
            (self.state / f'{service}.yaml').write_text(config)

    def run_update(self, service, fail=None):
        with mock.patch.object(
            local.subprocess, 'check_output', return_value='existing-container'
        ), mock.patch.object(local.subprocess, 'run', side_effect=fail) as run:
            if fail:
                with self.assertRaises(subprocess.CalledProcessError):
                    local.update(service, 2)
            else:
                local.update(service, 2)
            return [call.args[0][4:] for call in run.call_args_list]

    def test_only_selected_service_build_migration_restart_and_readiness(self):
        commands = self.run_update('shipments')
        self.assertEqual(
            commands,
            [
                [
                    'run',
                    '--rm',
                    '--no-deps',
                    '-e',
                    'NPROCS=2',
                    'prepare',
                    'bash',
                    'tools/local/build.sh',
                    'shipments',
                ],
                ['run', '--rm', '--no-deps', 'migrate-shipments'],
                ['up', '-d', '--no-deps', '--force-recreate', 'shipments'],
                [
                    'exec',
                    '-T',
                    'shipments',
                    'python3',
                    'tools/local/wait_ready.py',
                    '--port',
                    '8102',
                    '--timeout',
                    '120',
                ],
            ],
        )

    def test_bff_has_no_database_migration(self):
        commands = self.run_update('web_bff')
        self.assertEqual(len(commands), 3)
        self.assertFalse(any('migrate-web_bff' in command for command in commands))

    def test_failed_build_does_not_migrate_or_restart(self):
        commands = self.run_update('shipments', subprocess.CalledProcessError(1, 'build'))
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][-1], 'shipments')

    def test_rejects_unknown_service_and_missing_stand_before_build(self):
        with mock.patch.object(
            local.subprocess, 'check_output', return_value=''
        ) as check, mock.patch.object(local.subprocess, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'Неизвестный'):
                local.update('postgres', 2)
            check.assert_not_called()
            with self.assertRaisesRegex(ValueError, 'отсутствует'):
                local.update('shipments', 2)
            run.assert_not_called()

    def test_readiness_failure_prints_only_selected_service_logs(self):
        def execute(command, **kwargs):
            if 'exec' in command:
                raise subprocess.CalledProcessError(1, command)

        with mock.patch.object(
            local.subprocess, 'check_output', return_value='container'
        ), mock.patch.object(local.subprocess, 'run', side_effect=execute) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                local.update('shipments', 2)
            self.assertEqual(
                run.call_args.args[0][-5:], ['logs', '--no-color', '--tail', '50', 'shipments']
            )


class ReadyTest(unittest.TestCase):
    def test_retries_until_ready(self):
        response = mock.MagicMock()
        response.__enter__.return_value.status = 200
        with mock.patch.object(
            ready.urllib.request, 'urlopen', side_effect=[OSError('starting'), response]
        ) as request, mock.patch.object(ready.time, 'sleep'):
            ready.wait(8102, 10)
        self.assertEqual(request.call_count, 2)

    def test_unavailable_service_has_deadline(self):
        with mock.patch.object(
            ready.time, 'monotonic', side_effect=[0, 0, 0, 1, 2]
        ), mock.patch.object(ready.time, 'sleep'), mock.patch.object(
            ready.urllib.request, 'urlopen', side_effect=OSError('unavailable')
        ):
            with self.assertRaisesRegex(RuntimeError, 'Readiness не достигнута'):
                ready.wait(8102, 1)


class LocalTestsTest(unittest.TestCase):
    def test_make_selects_service_and_contracts_are_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tools').mkdir()
            (root / 'tools/versions.env').write_text('')
            (root / 'scripts').mkdir()
            shutil.copy(ROOT / 'scripts/ci-tests.py', root / 'scripts')
            (root / '.venv-gen/bin').mkdir(parents=True)
            for name, source in [
                ('python', sys.executable),
                ('cmake', shutil.which('cmake')),
                ('ctest', shutil.which('ctest')),
            ]:
                self.assertIsNotNone(source)
                (root / '.venv-gen/bin' / name).symlink_to(source)
            (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(local_tests NONE)
enable_testing()
foreach(name alpha beta alpha_unittest beta_unittest contract_service client_contract_unittest)
  add_custom_target(${name} ALL COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/${name}.built)
endforeach()
foreach(name alpha_unittest beta_unittest client_contract_unittest testsuite-contract_service)
  add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} -E true)
endforeach()
''')
            subprocess.run(
                [shutil.which('cmake'), '-S', str(root), '-B', str(root / 'build-debug')],
                check=True,
                capture_output=True,
            )
            driver = root / 'Makefile'
            driver.write_text(f'include {ROOT}/Makefile\ncmake-debug:\n\t@true\n')
            for shared, targets in [
                (0, {'alpha', 'alpha_unittest'}),
                (1, {'alpha', 'alpha_unittest', 'contract_service', 'client_contract_unittest'}),
            ]:
                for path in (root / 'build-debug').glob('*.built'):
                    path.unlink()
                subprocess.run(
                    [
                        'make',
                        '-f',
                        str(driver),
                        'test-debug',
                        f'REPO_ROOT={root}',
                        'SERVICE=alpha',
                        f'TEST_SHARED_CONTRACTS={shared}',
                    ],
                    cwd=root,
                    check=True,
                    capture_output=True,
                )
                self.assertEqual(
                    {path.stem for path in (root / 'build-debug').glob('*.built')}, targets
                )
