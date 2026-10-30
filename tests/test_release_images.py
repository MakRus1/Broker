"""Выбор образов по настоящему git diff и сохранение целостного релиза."""

import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile
import yaml

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


images = module('release_images', 'tools/deploy/build.py')
base_loader = module('release_base', 'tools/deploy/release_base.py')


def release(revision, services):
    return {
        'revision': revision,
        'images': {
            name: f"ghcr.io/makrus1/broker-{name.replace('_', '-')}@sha256:" + 'b' * 64
            for name in services
        },
    }


class ImageSelectionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for source in ROOT.glob('services/*/service.yaml'):
            target = self.root / source.relative_to(ROOT)
            target.parent.mkdir(parents=True)
            shutil.copy2(source, target)
        for source in ROOT.glob('libs/*/CMakeLists.txt'):
            target = self.root / source.relative_to(ROOT)
            target.parent.mkdir(parents=True)
            shutil.copy2(source, target)
        for path in ['.github/service-deps.yaml', 'scripts/affected-services.py']:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / path, target)
        self.git('init', '-q')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'user.name', 'Test')
        self.base = self.commit()
        self.services = images.discover(self.root)
        patch = mock.patch.object(images, 'ROOT', self.root)
        patch.start()
        self.addCleanup(patch.stop)

    def git(self, *args):
        return subprocess.check_output(['git', *args], cwd=self.root, text=True).strip()

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')
        return self.git('rev-parse', 'HEAD')

    def test_code_frontend_contract_library_and_packaging_changes(self):
        cases = [
            ('services/shipments/src/view.cpp', {'shipments'}),
            ('frontend/web/src/App.tsx', {'web_bff'}),
            ('frontend/admin/src/App.tsx', {'admin_bff'}),
            (
                'schemas/services/shipments/models.yaml',
                {'shipments', 'configs', 'admin_bff', 'web_bff', 'transportations', 'finance'},
            ),
            ('libs/common/include/shared.hpp', set(self.services)),
            ('tools/deploy/Dockerfile', set(self.services)),
            ('docs/readme.md', set()),
        ]
        for path, expected in cases:
            with self.subTest(path=path):
                self.git('reset', '--hard', self.base)
                target = self.root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('changed\n')
                revision = self.commit()
                chosen, _ = images.selected_services(
                    revision, self.services, release(self.base, self.services)
                )
                self.assertEqual(set(chosen), expected)

    def test_new_service_is_built_and_deleted_service_is_not_selected(self):
        old = release(self.base, self.services)
        path = self.root / 'services/new_service'
        path.mkdir()
        (path / 'service.yaml').write_text('version: 1\nlibraries: [common]\n')
        shutil.rmtree(self.root / 'services/broker')
        registry = self.root / '.github/service-deps.yaml'
        data = yaml.safe_load(registry.read_text())
        data['libs']['common']['services'].remove('broker')
        registry.write_text(yaml.safe_dump(data))
        revision = self.commit()
        chosen, _ = images.selected_services(revision, images.discover(self.root), old)
        self.assertIn('new_service', chosen)
        self.assertNotIn('broker', chosen)

    def test_missing_image_is_built_even_without_source_changes(self):
        previous = release(self.base, self.services)
        del previous['images']['broker']
        chosen, _ = images.selected_services(self.base, self.services, previous)
        self.assertEqual(chosen, ['broker'])

    def test_wrong_checkout_nonancestor_or_invalid_base_fails(self):
        base = release(self.base, self.services)
        with self.assertRaisesRegex(ValueError, 'checkout'):
            images.selected_services('a' * 40, self.services, base)
        base['images']['broker'] = 'ghcr.io/makrus1/broker-broker:latest'
        with self.assertRaises(ValueError):
            images.selected_services(self.base, self.services, base)
        base = release('a' * 40, self.services)
        with self.assertRaises(subprocess.CalledProcessError):
            images.selected_services(self.base, self.services, base)


class BuildImagesTest(unittest.TestCase):
    def test_only_selected_images_built_and_other_digests_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'release.json'
            services = images.discover(ROOT)
            previous = release('a' * 40, [*services, 'removed_service'])
            selected = ['broker', 'admin_bff', 'web_bff']

            def docker(command, **kwargs):
                metadata = Path(command[command.index('--metadata-file') + 1])
                metadata.write_text(json.dumps({'containerimage.digest': 'sha256:' + 'c' * 64}))

            with mock.patch.object(
                images, 'selected_services', return_value=(selected, {})
            ), mock.patch.object(
                images.subprocess, 'run', side_effect=docker
            ) as run, mock.patch.dict(
                os.environ, {'GITHUB_ACTIONS': 'true'}
            ):
                images.build('d' * 40, base=previous, output=output)
            result = json.loads(output.read_text())
            self.assertEqual(set(result), {'revision', 'images'})
            self.assertEqual(set(result['images']), set(services))
            self.assertEqual(run.call_count, len(selected))
            for service in services:
                self.assertTrue(
                    result['images'][service].endswith(('c' if service in selected else 'b') * 64)
                )
            for call, service, assets in zip(
                run.call_args_list, selected, ['assets-none', 'frontend-admin', 'frontend-web']
            ):
                command = call.args[0]
                self.assertIn(f'SERVICE={service}', command)
                self.assertIn(f'ASSETS_STAGE={assets}', command)
                self.assertIn('BUILD_SERVICES=' + ' '.join(selected), command)
                self.assertEqual('--cache-to' in command, service == selected[-1])
                self.assertIn(
                    'type=gha,version=2,scope=broker-services-selected-v2-linux-amd64',
                    command,
                )

    def test_no_changes_does_not_invoke_docker(self):
        with tempfile.TemporaryDirectory() as directory:
            previous = release('a' * 40, images.discover(ROOT))
            output = Path(directory) / 'release.json'
            with mock.patch.object(
                images, 'selected_services', return_value=([], {})
            ), mock.patch.object(images.subprocess, 'run') as run:
                images.build('b' * 40, base=previous, output=output)
            run.assert_not_called()
            self.assertEqual(json.loads(output.read_text())['images'], previous['images'])

    def test_failed_build_does_not_replace_release_file(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'release.json'
            output.write_text('previous output')
            with mock.patch.object(
                images, 'selected_services', return_value=(['broker'], {})
            ), mock.patch.object(
                images.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'docker')
            ):
                with self.assertRaises(subprocess.CalledProcessError):
                    images.build('a' * 40, output=output)
            self.assertEqual(output.read_text(), 'previous output')


class PreviousReleaseTest(unittest.TestCase):
    def archive(self, value):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as archive:
            archive.writestr('release.json', json.dumps(value))
        return data.getvalue()

    def test_skip_future_runs_and_expired_artifacts_then_download_valid_base(self):
        sha = 'a' * 40
        expected = release(sha, ['broker'])
        pages = [
            {'workflow_runs': [{'id': 3, 'head_sha': 'c' * 40}, {'id': 2, 'head_sha': 'b' * 40}]},
            {'artifacts': [{'id': 20, 'name': 'staging-release-' + 'b' * 40, 'expired': True}]},
            {'workflow_runs': [{'id': 1, 'head_sha': sha}]},
            {'artifacts': [{'id': 10, 'name': 'staging-release-' + sha, 'expired': False}]},
        ]
        with mock.patch.object(base_loader, 'api', side_effect=pages), mock.patch.object(
            base_loader.subprocess,
            'run',
            side_effect=[
                subprocess.CompletedProcess([], 1),
                subprocess.CompletedProcess([], 0),
                subprocess.CompletedProcess([], 0),
            ],
        ), mock.patch.object(
            base_loader.subprocess, 'check_output', return_value=self.archive(expected)
        ) as download:
            self.assertEqual(base_loader.previous_release('owner/repo', 'd' * 40), expected)
        self.assertIn('/artifacts/10/zip', download.call_args.args[0][-1])

    def test_no_base_is_explicit_but_api_errors_are_not_hidden(self):
        with mock.patch.object(base_loader, 'api', return_value={'workflow_runs': []}):
            self.assertIsNone(base_loader.previous_release('owner/repo', 'a' * 40))
        with mock.patch.object(base_loader, 'api', side_effect=RuntimeError('API failed')):
            with self.assertRaisesRegex(RuntimeError, 'API failed'):
                base_loader.previous_release('owner/repo', 'a' * 40)

    def test_artifact_revision_must_match_publication(self):
        pages = [
            {'workflow_runs': [{'id': 1, 'head_sha': 'a' * 40}]},
            {'artifacts': [{'id': 10, 'name': 'staging-release-' + 'a' * 40, 'expired': False}]},
        ]
        with mock.patch.object(base_loader, 'api', side_effect=pages), mock.patch.object(
            base_loader.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0)
        ), mock.patch.object(
            base_loader.subprocess,
            'check_output',
            return_value=self.archive(release('b' * 40, ['broker'])),
        ):
            with self.assertRaisesRegex(ValueError, 'revision'):
                base_loader.previous_release('owner/repo', 'c' * 40)
