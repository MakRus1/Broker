"""Проверка графа генерации настоящими командами CMake."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class CMakeTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cmake'), 'Нужен CMake')
    def test_codegen_target_preserves_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'source';build = Path(tmp) / 'build'
            (root / 'services').mkdir(parents=True)
            shutil.copy2(ROOT / '.clang-format', root / '.clang-format')
            shutil.copytree(ROOT / 'scripts/openapi_gen', root / 'scripts/openapi_gen', ignore=shutil.ignore_patterns('__pycache__'))
            shutil.copytree(ROOT / 'services/broker', root / 'services/broker', ignore=shutil.ignore_patterns('__pycache__'))
            shutil.copytree(ROOT / 'schemas', root / 'schemas')
            shutil.copytree(ROOT / 'cmake', root / 'cmake')
            shutil.copytree(ROOT / 'libs', root / 'libs')
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
            generated = build / 'generated/src/views/hello/get/handler.cpp'
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
            root.mkdir()
            for directory in ['scripts/openapi_gen', 'cmake', 'libs', 'schemas', 'services/broker', 'tests/fixtures/contract_service']:
                shutil.copytree(ROOT / directory, root / directory, ignore=shutil.ignore_patterns('__pycache__'))
            shutil.copy2(ROOT / '.clang-format', root / '.clang-format')
            (root / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(consumer LANGUAGES NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${CMAKE_SOURCE_DIR}/cmake/OpenApiCodegen.cmake")
broker_openapi_generate("${CMAKE_SOURCE_DIR}/tests/fixtures/contract_service")
''')
            generate = [sys.executable, str(root / 'scripts/openapi_gen/generate.py'), '--service-dir', str(root / 'tests/fixtures/contract_service')]
            def run(*args):
                return subprocess.run(args, check=True, capture_output=True)
            run(*generate)
            run('cmake', '-G', 'Ninja', '-S', str(root), '-B', str(build), f'-DPython3_EXECUTABLE={sys.executable}')
            run('cmake', '--build', str(build), '--target', 'consumer_codegen')
            header = build / 'generated/include/clients/broker/client.hpp'
            self.assertIn('helloGet', header.read_text())
            spec = root / 'schemas/services/broker/hello.yaml'
            spec.write_text(spec.read_text().replace('operationId: helloGet', 'operationId: HelloRenamed'))
            # Сборка обнаруживает устаревший lock, но не исправляет исходники.
            before = {p: p.read_bytes() for p in root.rglob('*') if p.is_file() and '__pycache__' not in str(p)}
            with self.assertRaises(subprocess.CalledProcessError):
                run('cmake', '--build', str(build), '--target', 'consumer_codegen')
            self.assertEqual(before, {p: p.read_bytes() for p in before})
            run(*generate)
            run('cmake', '--build', str(build), '--target', 'consumer_codegen')
            self.assertIn('HelloRenamed', header.read_text())
            self.assertNotIn('helloGet', header.read_text())
