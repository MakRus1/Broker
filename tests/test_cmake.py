"""Проверка графа генерации настоящими командами CMake."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests'))
from support.generator_repository import copy_generator_repository


class CMakeTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cmake'), 'Нужен CMake')
    def test_codegen_target_preserves_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'source';build = Path(tmp) / 'build'
            copy_generator_repository(root)
            (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(broker LANGUAGES NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_SOURCE_DIR}/services/broker")
''')
            def run(*args):
                subprocess.run(args, check=True, capture_output=True)
            run(sys.executable, str(root / 'scripts/openapi_gen/generate.py'), '--service-dir', str(root / 'services/broker'))
            before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file() and '__pycache__' not in str(p)}
            run('cmake', '-S', str(root), '-B', str(build), f'-DPython3_EXECUTABLE={sys.executable}')
            run('cmake', '--build', str(build), '--target', 'broker_codegen')
            generated = build / 'generated/src/views/hello/handler.cpp'
            self.assertTrue(generated.exists())
            generated.unlink()
            run('cmake', '--build', str(build), '--target', 'broker_codegen')
            self.assertTrue(generated.exists())
            after = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file() and '__pycache__' not in str(p)}
            self.assertEqual(before, after)

    @unittest.skipUnless(shutil.which('cmake'), 'Нужен CMake')
    def test_client_rebuilds_on_provider_contract_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'source';build = Path(tmp) / 'build'
            copy_generator_repository(root)
            shutil.copytree(ROOT / 'tests/fixtures/contract_service', root / 'tests/fixtures/contract_service',
                            ignore=shutil.ignore_patterns('__pycache__'))
            manifest_path = root / 'tests/fixtures/contract_service/service.yaml'
            manifest = yaml.safe_load(manifest_path.read_text())
            manifest.pop('critical_dependencies')
            manifest_path.write_text(yaml.safe_dump(manifest))
            (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(consumer LANGUAGES NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_SOURCE_DIR}/tests/fixtures/contract_service")
''')
            generate = [sys.executable, str(root / 'scripts/openapi_gen/generate.py'), '--service-dir', str(root / 'tests/fixtures/contract_service')]
            def run(*args):
                result = subprocess.run(args, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result
            run(*generate)
            run('cmake', '-G', 'Ninja', '-S', str(root), '-B', str(build), f'-DPython3_EXECUTABLE={sys.executable}')
            run('cmake', '--build', str(build), '--target', 'consumer_codegen')
            header = build / 'generated/include/clients/broker/client.hpp'
            self.assertIn('Hello', header.read_text())
            spec = root / 'schemas/services/broker/hello.yaml'
            spec.write_text(spec.read_text().replace('  /hello:', '  /hello-renamed:'))
            # Клиент обновляется при сборке без изменения lock или исходников.
            before = {p: p.read_bytes() for p in root.rglob('*') if p.is_file() and '__pycache__' not in str(p)}
            run('cmake', '--build', str(build), '--target', 'consumer_codegen')
            self.assertEqual(before, {p: p.read_bytes() for p in before})
            self.assertIn('HelloRenamed', header.read_text())
            self.assertNotIn('namespace Hello {', header.read_text())
