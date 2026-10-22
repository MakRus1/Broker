"""Выбор дорогих тестов не должен расширяться из-за изменений интерфейса."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ci_tools', ROOT / 'scripts/ci-tools.py')
tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tools)
ALL = sorted(path.stem for path in (ROOT / 'tests').glob('test_*.py'))


class SelectionTest(unittest.TestCase):
    def test_frontend_and_documents_do_not_run_generator(self):
        self.assertEqual(
            tools.select(
                ['frontend/web/src/Select.tsx', 'frontend/web/package.json', 'docs/staging.md'], ALL
            ),
            {},
        )

    def test_business_implementation_does_not_run_generator(self):
        self.assertEqual(
            tools.select(
                [
                    'services/counterparties/src/components/dadata.cpp',
                    'services/counterparties/tests/test_registry.py',
                ],
                ALL,
            ),
            {},
        )

    def test_generator_selects_scaffolds_and_cmake(self):
        chosen = tools.select(['scripts/openapi_gen/models.py'], ALL)
        self.assertIn('test_services', chosen)
        self.assertIn('test_cmake', chosen)
        self.assertNotIn('test_staging_network', chosen)

    def test_infrastructure_does_not_run_scaffolds(self):
        chosen = tools.select(['scripts/staging_manifest.py'], ALL)
        self.assertIn('test_deployment', chosen)
        self.assertNotIn('test_services', chosen)

    def test_manifest_checks_generator_and_deployment(self):
        chosen = tools.select(['services/broker/service.yaml'], ALL)
        self.assertIn('test_services', chosen)
        self.assertIn('test_deployment', chosen)

    def test_test_change_runs_itself(self):
        self.assertEqual(set(tools.select(['tests/test_models.py'], ALL)), {'test_models'})

    def test_missing_base_and_unknown_tool_are_safe(self):
        for paths in [None, ['scripts/new_tool.py'], ['.github/workflows/ci.yml']]:
            with self.subTest(paths=paths):
                self.assertEqual(set(tools.select(paths, ALL)), set(ALL))
