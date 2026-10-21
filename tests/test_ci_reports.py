"""Проверки отчетов CI на настоящем CTest: падение не скрывает остальные сервисы."""

import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('ci_tests', ROOT / 'scripts/ci-tests.py')
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class CiReportsTest(unittest.TestCase):
    def test_failed_service_does_not_skip_others_and_reports_are_downloadable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'CMakeLists.txt').write_text('''
cmake_minimum_required(VERSION 3.24)
project(ci_reports NONE)
enable_testing()
add_test(NAME alpha_unittest COMMAND "${PYTHON}" -c "raise SystemExit(1)")
add_test(NAME testsuite-beta COMMAND "${PYTHON}" -c "print('beta passed')")
add_test(NAME testsuite-contract_service COMMAND "${PYTHON}" -c "print('contract passed')")
add_test(NAME client_contract_unittest COMMAND "${PYTHON}" -c "print('client passed')")
''')
            cmake = shutil.which('cmake')
            ctest = shutil.which('ctest')
            self.assertIsNotNone(cmake, 'CMake должен быть установлен из requirements.txt')
            self.assertIsNotNone(ctest, 'CTest должен быть установлен из requirements.txt')
            build = root / 'build'
            subprocess.run(
                [str(cmake), '-S', str(source), '-B', str(build), f'-DPYTHON={sys.executable}'],
                check=True,
                capture_output=True,
            )
            output = root / 'results'
            command = [
                sys.executable,
                str(ROOT / 'scripts/ci-tests.py'),
                'run',
                '--build-dir',
                str(build),
                '--ctest',
                str(ctest),
                '--output-dir',
                str(output),
                '--services',
                'alpha',
                'beta',
            ]
            process = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(process.returncode, 1, process.stderr)
            results = json.loads((output / 'results.json').read_text())
            self.assertEqual([item['status'] for item in results], ['failed', 'passed', 'passed'])
            self.assertEqual(results[0]['ctest']['failed'], 1)
            self.assertEqual(results[1]['ctest']['passed'], 1)
            self.assertEqual(results[2]['ctest']['passed'], 2)
            self.assertIn('beta passed', (output / 'beta.log').read_text())
            self.assertTrue((output / 'alpha.xml').is_file())
            summary = root / 'complete.md'
            subprocess.run(
                [
                    sys.executable,
                    str(ROOT / 'scripts/ci-tests.py'),
                    'summary',
                    '--output-dir',
                    str(output),
                    '--services',
                    'alpha',
                    'beta',
                    '--preset',
                    'debug',
                    '--arch',
                    'X64',
                    '--build-result',
                    'success',
                    '--test-result',
                    'failure',
                    '--summary',
                    str(summary),
                ],
                check=True,
                capture_output=True,
            )
            self.assertIn('| `alpha` | ❌ Ошибка | 0 / 1 / 0 |', summary.read_text())
            self.assertIn('| `beta` | ✅ Пройдены | 1 / 0 / 0 |', summary.read_text())
            # Отчет из прошлого запуска не может сделать отсутствие тестов зеленым.
            missing = subprocess.run([*command[:-2], 'missing'], text=True, capture_output=True)
            self.assertNotEqual(missing.returncode, 0)
            self.assertIn('Не найдены тесты', missing.stderr)
            self.assertEqual(json.loads((output / 'results.json').read_text()), [])
            report = root / 'summary.md'
            subprocess.run(
                [
                    sys.executable,
                    str(ROOT / 'scripts/ci-tests.py'),
                    'summary',
                    '--output-dir',
                    str(output),
                    '--services',
                    'missing',
                    '--preset',
                    'debug',
                    '--arch',
                    'X64',
                    '--build-result',
                    'success',
                    '--test-result',
                    'failure',
                    '--summary',
                    str(report),
                ],
                check=True,
                capture_output=True,
            )
            self.assertIn('Не запущены', report.read_text())
            self.assertIn('failure', report.read_text())

    def test_junit_counts_failures_errors_and_skips(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'pytest.xml'
            path.write_text('''<testsuites><testsuite>
<testcase name="ok"/><testcase name="failure"><failure/></testcase>
<testcase name="error"><error/></testcase><testcase name="skip"><skipped/></testcase>
</testsuite></testsuites>''')
            self.assertEqual(ci.counts(path), {'passed': 1, 'failed': 2, 'skipped': 1})
            path.write_text('<testsuite/>')
            with self.assertRaisesRegex(ValueError, 'нет тестов'):
                ci.counts(path)

    def test_plan_does_not_hide_detection_errors(self):
        args = type('Args', (), {'base': 'base', 'summary': None, 'github_output': None})()
        with mock.patch.object(
            ci.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'git')
        ):
            with self.assertRaises(subprocess.CalledProcessError):
                ci.plan(args)

    def test_plan_expands_all_and_reports_documentation_only_skip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'services/alpha').mkdir(parents=True)
            (root / 'services/beta').mkdir()
            args = type(
                'Args',
                (),
                {
                    'base': 'missing',
                    'summary': root / 'summary.md',
                    'github_output': root / 'output',
                },
            )()
            for value, expected in [('all', 'alpha beta'), ('', '')]:
                with self.subTest(value=value), mock.patch.object(
                    ci, 'ROOT', root
                ), mock.patch.object(
                    ci, 'changed_paths', return_value=['README.md']
                ), mock.patch.object(
                    ci.subprocess, 'run', return_value=mock.Mock(stdout=value)
                ):
                    ci.plan(args)
                    self.assertTrue(
                        args.github_output.read_text().endswith(f'services={expected}\n')
                    )
            self.assertIn('пропускаются', args.summary.read_text())
