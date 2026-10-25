"""Выбор сервисов проверяется настройкой настоящего CMake и инвентарём CTest."""

import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('cmake') and shutil.which('ctest'), 'Нужны CMake и CTest')
class SelectedServicesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        for name, directory in [
            ('alpha', 'services/alpha'),
            ('beta', 'services/beta'),
            ('contract_service', 'tests/fixtures/contract_service'),
        ]:
            path = self.root / directory
            path.mkdir(parents=True)
            if name != 'contract_service':
                (path / 'service.yaml').write_text(f'name: {name}\n')
            (path / 'CMakeLists.txt').write_text(
                f'add_custom_target({name})\n'
                'if(BUILD_TESTING)\n'
                f'  add_test(NAME testsuite-{name} COMMAND "${{CMAKE_COMMAND}}" -E true)\n'
                'endif()\n'
            )
        (self.root / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\n'
            'project(selection NONE)\n'
            'include(CTest)\n'
            f'include("{ROOT}/cmake/SelectedServices.cmake")\n'
            'broker_add_selected_services()\n'
        )

    def configure(self, *options, success=True):
        result = subprocess.run(
            ['cmake', '-S', str(self.root), '-B', str(self.build), *options],
            capture_output=True,
            text=True,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            inventory = subprocess.check_output(
                ['ctest', '--test-dir', str(self.build), '--show-only=json-v1'], text=True
            )
            return {test['name'] for test in json.loads(inventory)['tests']}
        self.assertNotEqual(result.returncode, 0)
        return result.stdout + result.stderr

    def test_default_configures_all_services_and_contracts(self):
        self.assertEqual(
            self.configure(), {'testsuite-alpha', 'testsuite-beta', 'testsuite-contract_service'}
        )

    def test_switching_selection_does_not_retain_previous_services(self):
        self.configure()
        self.assertEqual(
            self.configure('-DBROKER_SERVICES=alpha', '-DBROKER_SHARED_CONTRACTS=OFF'),
            {'testsuite-alpha'},
        )
        self.assertEqual(self.configure('-DBROKER_SERVICES=beta'), {'testsuite-beta'})
        self.assertEqual(
            self.configure('-DBROKER_SERVICES=', '-DBROKER_SHARED_CONTRACTS=ON'),
            {'testsuite-contract_service'},
        )
        self.build = self.root / 'build-without-tests'
        self.assertEqual(self.configure('-DBROKER_SERVICES=all', '-DBUILD_TESTING=OFF'), set())

    def test_unselected_service_is_not_configured(self):
        (self.root / 'services/beta/CMakeLists.txt').write_text('message(FATAL_ERROR "beta read")')
        self.assertEqual(
            self.configure('-DBROKER_SERVICES=alpha', '-DBROKER_SHARED_CONTRACTS=OFF'),
            {'testsuite-alpha'},
        )

    def test_multiple_services_with_spaces_or_semicolons_and_duplicates(self):
        for names in ['alpha beta alpha', 'alpha;beta;alpha']:
            self.assertEqual(
                self.configure(f'-DBROKER_SERVICES={names}', '-DBROKER_SHARED_CONTRACTS=OFF'),
                {'testsuite-alpha', 'testsuite-beta'},
            )

    def test_unknown_service_is_an_error(self):
        output = self.configure('-DBROKER_SERVICES=alpha missing', success=False)
        self.assertIn('Неизвестный сервис: missing', output)


class MakeSelectionTest(unittest.TestCase):
    def test_make_passes_local_ci_and_full_selections(self):
        for goal, variables, selected, contracts in [
            ('test-debug', ['SERVICE=shipments'], 'shipments', 'OFF'),
            ('test-debug', ['SERVICE=shipments', 'TEST_SHARED_CONTRACTS=1'], 'shipments', 'ON'),
            ('test-all-debug', ['SERVICE=shipments'], 'all', 'ON'),
            (
                'ci-build-debug',
                ['CI_BUILD_SERVICES=shipments transportations', 'CI_SHARED_CONTRACTS=false'],
                'shipments transportations',
                'OFF',
            ),
            ('ci-build-debug', ['CI_BUILD_SERVICES=', 'CI_SHARED_CONTRACTS=true'], '', 'ON'),
        ]:
            with self.subTest(goal=goal, variables=variables):
                output = subprocess.check_output(
                    ['make', '--dry-run', goal, *variables], cwd=ROOT, text=True
                )
                configure = next(line for line in output.splitlines() if '--preset debug' in line)
                arguments = shlex.split(configure)
                self.assertIn(f'-DBROKER_SERVICES={selected}', arguments)
                self.assertIn(f'-DBROKER_SHARED_CONTRACTS={contracts}', arguments)
