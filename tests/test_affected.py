"""Проверки CI на настоящем временном Git-репозитории."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parent.parent


class AffectedTest(unittest.TestCase):
    def test_diff_errors_and_dependency_selection(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / 'scripts').mkdir()
            (repo / '.github').mkdir()
            (repo / 'services/broker').mkdir(parents=True)
            shutil.copy(ROOT / 'scripts/affected-services.py', repo / 'scripts')
            shutil.copy(ROOT / '.github/service-deps.yaml', repo / '.github')
            deps = yaml.safe_load((repo / '.github/service-deps.yaml').read_text())
            for entry in deps['libs'].values():
                entry['services'] = ['broker']
            (repo / '.github/service-deps.yaml').write_text(yaml.safe_dump(deps))

            def git(*args):
                return (
                    subprocess.run(['git', '-C', str(repo), *args], check=True, capture_output=True)
                    .stdout.decode()
                    .strip()
                )

            git('init')
            git('config', 'user.name', 'Тест')
            git('config', 'user.email', 'test@example.invalid')
            (repo / 'services/broker/base').write_text('initial')
            git('add', '.')
            git('commit', '-m', 'Начало')
            script = [sys.executable, str(repo / 'scripts/affected-services.py')]
            self.assertEqual(subprocess.check_output(script + ['missing']).decode().strip(), 'all')
            for path, expected in [
                ('README.md', ''),
                ('services/broker/view.cpp', 'broker'),
                ('libs/common/a.hpp', 'broker'),
                ('tools/version', ''),
            ]:
                base = git('rev-parse', 'HEAD')
                target = repo / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('change')
                git('add', '.')
                git('commit', '-m', 'Изменение')
                self.assertEqual(
                    subprocess.check_output(script + [base]).decode().strip(), expected
                )
            # Manifest заменяет старую карту и подключает новую библиотеку.
            (repo / 'libs/extra').mkdir(parents=True)
            (repo / 'libs/extra/CMakeLists.txt').write_text('add_library(broker_extra INTERFACE)')
            (repo / 'libs/common/CMakeLists.txt').write_text('add_library(broker_common INTERFACE)')
            (repo / 'services/other').mkdir()
            (repo / 'services/broker/service.yaml').write_text(
                'version: 1\nlibraries: [common, extra]\n'
            )
            (repo / 'services/other/service.yaml').write_text('version: 1\nlibraries: [common]\n')
            git('add', '.')
            git('commit', '-m', 'Manifest зависимостей')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/extra/a.hpp').write_text('change')
            git('add', '.')
            git('commit', '-m', 'Изменение дополнительной библиотеки')
            self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), 'broker')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/common/a.hpp').write_text('second change')
            git('add', '.')
            git('commit', '-m', 'Изменение общей библиотеки')
            self.assertEqual(
                subprocess.check_output(script + [base]).decode().strip(), 'broker other'
            )
            (repo / 'libs/configs').mkdir(parents=True)
            (repo / 'libs/configs/CMakeLists.txt').write_text(
                'add_library(broker_configs INTERFACE)'
            )
            # Секция auth подключает библиотеку неявно, CI тоже должен учитывать ее изменения.
            (repo / 'libs/auth').mkdir(parents=True)
            (repo / 'libs/auth/CMakeLists.txt').write_text('add_library(broker_auth INTERFACE)')
            manifest = repo / 'services/broker/service.yaml'
            manifest.write_text(
                manifest.read_text()
                + 'auth: {audience: api, roles-client: api, protected-prefix: /api/, permissions: {}}\n'
            )
            git('add', '.')
            git('commit', '-m', 'Авторизация из манифеста')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/auth/a.hpp').write_text('auth change')
            git('add', '.')
            git('commit', '-m', 'Изменение библиотеки авторизации')
            self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), 'broker')
            # Публичный контракт затрагивает поставщика и транзитивных потребителей.
            (repo / 'services/third').mkdir()
            (repo / 'services/other/service.yaml').write_text(
                'version: 1\nlibraries: [common]\nclients: {api: {service: broker, transport: http}}\n'
            )
            (repo / 'services/third/service.yaml').write_text(
                'version: 1\nlibraries: [common]\nclients: {api: {service: other, transport: http}}\n'
            )
            # Взаимная ссылка — HTTP-зависимость, а не цикл CMake.
            (repo / 'services/broker/service.yaml').write_text(
                'version: 1\nlibraries: [common, extra]\nclients: {api: {service: third, transport: http}}\n'
            )
            git('add', '.')
            git('commit', '-m', 'Клиенты')
            base = git('rev-parse', 'HEAD')
            contract = repo / 'schemas/services/broker/openapi.yaml'
            contract.parent.mkdir(parents=True)
            contract.write_text('contract')
            git('add', '.')
            git('commit', '-m', 'Изменение публичного контракта')
            self.assertEqual(
                subprocess.check_output(script + [base]).decode().strip(), 'broker other third'
            )
            # Внутренняя реализация не меняет сгенерированные клиенты потребителей.
            base = git('rev-parse', 'HEAD')
            (repo / 'services/broker/view.cpp').write_text('implementation only')
            git('add', '.')
            git('commit', '-m', 'Только реализация')
            self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), 'broker')
            for path in [
                'scripts/staging_manifest.py',
                'tests/test_deployment.py',
                'tools/local/check.py',
            ]:
                base = git('rev-parse', 'HEAD')
                target = repo / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('tool change')
                git('add', '.')
                git('commit', '-m', 'Инструменты окружения')
                self.assertEqual(subprocess.check_output(script + [base]).decode().strip(), '')
            # Зависимость через промежуточную библиотеку также затрагивает сервис.
            (repo / 'libs/outer').mkdir()
            (repo / 'libs/outer/CMakeLists.txt').write_text(
                'add_library(broker_outer INTERFACE)\ntarget_link_libraries(broker_outer INTERFACE broker_extra)'
            )
            manifest = repo / 'services/other/service.yaml'
            manifest.write_text(manifest.read_text().replace('[common]', '[common, outer]'))
            git('add', '.')
            git('commit', '-m', 'Транзитивная библиотека')
            base = git('rev-parse', 'HEAD')
            (repo / 'libs/extra/a.hpp').write_text('transitive change')
            git('add', '.')
            git('commit', '-m', 'Общая зависимость')
            self.assertEqual(
                subprocess.check_output(script + [base]).decode().strip(), 'broker other'
            )
            # Схема конфига и его контекст затрагивают Configs и явного потребителя.
            (repo / 'services/configs').mkdir()
            (repo / 'services/configs/service.yaml').write_text('version: 1\nlibraries: [common]\n')
            manifest = repo / 'services/broker/service.yaml'
            manifest.write_text(manifest.read_text() + 'configs: [RULES]\n')
            schema = repo / 'schemas/configs/example/RULES.yaml'
            schema.parent.mkdir(parents=True)
            schema.write_text('name: RULES\ncontext: example/CONTEXT\n')
            context = repo / 'schemas/config_contexts/example/CONTEXT.yaml'
            context.parent.mkdir(parents=True)
            context.write_text('name: CONTEXT\n')
            git('add', '.')
            git('commit', '-m', 'Схемы конфигурации')
            for path in [schema, context]:
                base = git('rev-parse', 'HEAD')
                path.write_text(path.read_text() + '# change\n')
                git('add', '.')
                git('commit', '-m', 'Изменение схемы')
                self.assertEqual(
                    subprocess.check_output(script + [base]).decode().strip(), 'broker configs'
                )
            (repo / '.github/service-deps.yaml').write_text('libs: broken\nshared_paths: []\n')
            self.assertNotEqual(
                subprocess.run(script + ['HEAD'], capture_output=True).returncode, 0
            )
            shutil.copy(ROOT / '.github/service-deps.yaml', repo / '.github/service-deps.yaml')
            shutil.rmtree(repo / '.git')
            # Отсутствие репозитория нельзя трактовать как «нет изменений».
            self.assertNotEqual(
                subprocess.run(script + ['HEAD'], capture_output=True).returncode, 0
            )
