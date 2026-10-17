"""Проверки CI на настоящем временном Git-репозитории."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class AffectedTest(unittest.TestCase):
    def test_diff_errors_and_dependency_selection(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / 'scripts').mkdir(); (repo / '.github').mkdir(); (repo / 'services/broker').mkdir(parents=True)
            shutil.copy(ROOT / 'scripts/affected-services.py', repo / 'scripts')
            shutil.copy(ROOT / '.github/service-deps.yaml', repo / '.github')
            def git(*args):
                return subprocess.run(['git', '-C', str(repo), *args], check=True, capture_output=True).stdout.decode().strip()
            git('init');git('config', 'user.name', 'Тест');git('config', 'user.email', 'test@example.invalid')
            (repo / 'services/broker/base').write_text('initial')
            git('add', '.');git('commit', '-m', 'Начало')
            script = [sys.executable, str(repo / 'scripts/affected-services.py')]
            self.assertEqual(subprocess.check_output(script + ['missing']).decode().strip(), 'all')
            for path, expected in [('README.md', ''), ('services/broker/view.cpp', 'broker'), ('libs/common/a.hpp', 'broker'), ('tools/version', 'all')]:
                base = git('rev-parse', 'HEAD')
                target = repo / path;target.parent.mkdir(parents=True, exist_ok=True);target.write_text('change')
                git('add', '.');git('commit', '-m', 'Изменение')
                self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), expected)
            # Manifest заменяет старую карту и подключает новую библиотеку.
            (repo / 'libs/extra').mkdir(parents=True)
            (repo / 'libs/extra/CMakeLists.txt').write_text('add_library(broker_extra INTERFACE)')
            (repo / 'libs/common/CMakeLists.txt').write_text('add_library(broker_common INTERFACE)')
            (repo / 'services/other').mkdir()
            (repo / 'services/broker/service.yaml').write_text('version: 1\nlibraries: [common, extra]\n')
            (repo / 'services/other/service.yaml').write_text('version: 1\nlibraries: [common]\n')
            git('add', '.');git('commit', '-m', 'Manifest зависимостей')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/extra/a.hpp').write_text('change')
            git('add', '.');git('commit', '-m', 'Изменение дополнительной библиотеки')
            self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), 'broker')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/common/a.hpp').write_text('second change')
            git('add', '.');git('commit', '-m', 'Изменение общей библиотеки')
            self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), 'broker other')
            (repo / '.github/service-deps.yaml').write_text('libs: broken\nshared_paths: []\n')
            self.assertNotEqual(subprocess.run(script + ['HEAD'], capture_output=True).returncode, 0)
            shutil.copy(ROOT / '.github/service-deps.yaml', repo / '.github/service-deps.yaml')
            shutil.rmtree(repo / '.git')
            # Отсутствие репозитория нельзя трактовать как «нет изменений».
            self.assertNotEqual(subprocess.run(script + ['HEAD'], capture_output=True).returncode, 0)
