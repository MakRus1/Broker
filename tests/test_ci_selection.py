"""План CI: дорогие проверки включаются только для соответствующих изменений."""

import importlib.util
from pathlib import Path
import unittest
import shutil
import subprocess
import tempfile
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ci_selection', ROOT / 'scripts/ci-tests.py')
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class CiSelectionTest(unittest.TestCase):
    def test_documentation(self):
        self.assertEqual(
            ci.select_checks(['docs/web-ui.md'], []),
            {'scaffolds': False, 'smoke': False, 'frontends': [], 'build_services': []},
        )

    def test_service_change_does_not_build_scaffolds(self):
        plan = ci.select_checks(['services/shipments/src/view.cpp'], ['shipments', 'web_bff'])
        self.assertFalse(plan['scaffolds'])
        self.assertTrue(plan['smoke'])
        self.assertEqual(plan['frontends'], ['web'])
        self.assertEqual(plan['build_services'], ['shipments', 'transportations', 'web_bff'])

    def test_frontend_only(self):
        plan = ci.select_checks(['frontend/admin/src/App.tsx'], [])
        self.assertEqual(plan['frontends'], ['admin'])
        self.assertFalse(plan['scaffolds'])
        self.assertFalse(plan['smoke'])
        self.assertEqual(plan['build_services'], [])

    def test_generator_and_build_changes_enable_scaffolds(self):
        for path in [
            'scripts/openapi_gen/features.py',
            'scripts/templates/service/service.yaml',
            'scripts/create_service.py',
            'CMakeLists.txt',
            'tools/versions.env',
            '.github/workflows/ci.yml',
        ]:
            with self.subTest(path=path):
                self.assertTrue(ci.select_checks([path], ['configs'])['scaffolds'])

    def test_missing_base_runs_everything(self):
        plan = ci.select_checks(None, ['broker', 'configs', 'shipments', 'transportations'])
        self.assertTrue(plan['scaffolds'])
        self.assertTrue(plan['smoke'])
        self.assertEqual(plan['frontends'], ['admin', 'web'])

    def test_build_targets_exclude_unaffected_services(self):
        args = type(
            'Args',
            (),
            {
                'services': ['configs'],
                'build_dir': Path('/tmp/build'),
                'ctest': 'ctest',
                'cmake': 'cmake',
                'jobs': 4,
            },
        )()
        with mock.patch.object(
            ci.subprocess,
            'check_output',
            return_value='{"tests":['
            '{"name":"configs_unittest"},{"name":"broker_unittest"},'
            '{"name":"client_contract_unittest"}]}',
        ), mock.patch.object(ci.subprocess, 'run') as run:
            ci.build(args)
        targets = run.call_args.args[0]
        self.assertIn('configs', targets)
        self.assertIn('configs_unittest', targets)
        self.assertIn('contract_service', targets)
        self.assertIn('client_contract_unittest', targets)
        self.assertNotIn('broker', targets)
        self.assertNotIn('broker_unittest', targets)

    def test_real_cmake_builds_only_selected_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'CMakeLists.txt').write_text("""
cmake_minimum_required(VERSION 3.24)
project(selective_ci NONE)
enable_testing()
foreach(name alpha beta alpha_unittest beta_unittest contract_service client_contract_unittest)
    add_custom_target(${name} ALL COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/${name}.built)
endforeach()
add_test(NAME alpha_unittest COMMAND ${CMAKE_COMMAND} -E true)
add_test(NAME beta_unittest COMMAND ${CMAKE_COMMAND} -E true)
add_test(NAME client_contract_unittest COMMAND ${CMAKE_COMMAND} -E true)
""")
            cmake, ctest = shutil.which('cmake'), shutil.which('ctest')
            self.assertIsNotNone(cmake)
            self.assertIsNotNone(ctest)
            build = root / 'build'
            subprocess.run(
                [cmake, '-S', str(source), '-B', str(build)], check=True, capture_output=True
            )
            args = type(
                'Args',
                (),
                {
                    'services': ['alpha'],
                    'build_dir': build,
                    'ctest': ctest,
                    'cmake': cmake,
                    'jobs': 2,
                },
            )()
            ci.build(args)
            self.assertEqual(
                {path.name for path in build.glob('*.built')},
                {
                    'alpha.built',
                    'alpha_unittest.built',
                    'contract_service.built',
                    'client_contract_unittest.built',
                },
            )
