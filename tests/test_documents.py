"""Файловые ссылки, перенос зависимостей и неизменность исходников OpenAPI."""
from pathlib import Path
import sys
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import load, run, digest, specification_files
from export_clients import export


def response(ref):
    return {'get': {'responses': {'200': {'description': 'Успех',
            'content': {'application/json': {'schema': {'$ref': ref}}}}}}}


class DocumentsTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.service = self.root / 'services/example'
        self.api = self.service / 'docs/api'
        self.components = self.service / 'docs/components/schemas'
        self.components.mkdir(parents=True)
        (self.api / 'foo/bar').mkdir(parents=True)
        self.source = self.api / 'foo/bar/show.yaml'
        self.write(self.components / 'child.yaml', {'Child': {
            'type': 'object', 'additionalProperties': False,
            'required': ['name'], 'properties': {'name': {'type': 'string', 'minLength': 1}}}})
        self.write(self.components / 'envelope.yaml', {'Envelope': {
            'type': 'object', 'properties': {'child': {'$ref': 'child.yaml#/Child'}},
            'required': ['child'], 'additionalProperties': False}})
        self.data = {'openapi': '3.0.3', 'paths': {
            '/foo/bar/show': response('../../../components/schemas/envelope.yaml#/Envelope')}}
        self.write(self.source, self.data)
        (self.service / 'client-api.yaml').write_text('version: 1\ninclude: [/**]\n')

    def write(self, path, data):
        path.write_text(yaml.safe_dump(data, allow_unicode=True, sort_keys=False))

    def test_external_schemas_export_and_client_loading(self):
        operation, = load(self.service)
        self.assertEqual({model.name for model in operation.models}, {'Envelope', 'Child'})
        export(self.service)
        public = self.root / 'schemas/services/example'
        exported = public / 'foo/bar/show.yaml'
        self.assertIn('../../components/schemas/envelope.yaml#/Envelope', exported.read_text())
        self.assertTrue((public / 'components/schemas/child.yaml').is_file())
        self.assertEqual(load(self.service, public_only=True), [operation])
        self.assertIn(public / 'components/schemas/child.yaml',
                      specification_files(self.service, public_only=True))
        export(self.service, check=True)
        before = exported.read_bytes()
        child = self.components / 'child.yaml'
        child.write_text(child.read_text().replace('minLength: 1', 'minLength: 2'))
        with self.assertRaises(ValueError):
            export(self.service, check=True)
        self.assertEqual(exported.read_bytes(), before)

    def test_codegen_tracks_components_without_mutating_sources(self):
        (self.service / 'configs').mkdir()
        (self.service / 'configs/static_config.yaml').write_text(
            '# OPENAPI_HANDLERS_BEGIN\n# OPENAPI_HANDLERS_END\n')
        run(self.service)
        def snapshot():
            return {p: p.read_bytes() for p in self.service.rglob('*') if p.is_file()}
        before = snapshot()
        run(self.service, check=True)
        output = self.root / 'generated'
        run(self.service, output=output)
        generated = {p: p.read_bytes() for p in output.rglob('*') if p.is_file()}
        run(self.service, output=output)
        self.assertEqual(generated, {p: p.read_bytes() for p in output.rglob('*') if p.is_file()})
        self.assertEqual(snapshot(), before)
        old_digest = digest(self.service, load(self.service))
        child = self.components / 'child.yaml'
        child.write_text(child.read_text().replace('minLength: 1', 'minLength: 2'))
        self.assertNotEqual(old_digest, digest(self.service, load(self.service)))
        with self.assertRaisesRegex(ValueError, 'codegen.lock'):
            run(self.service, check=True)

    def test_filter_exports_only_needed_component_files(self):
        self.write(self.components / 'private.yaml', {'Private': {'type': 'object', 'properties': {}}})
        self.data['paths']['/admin/internal'] = response('../../../components/schemas/private.yaml#/Private')
        self.write(self.source, self.data)
        export(self.service)
        self.assertTrue((self.root / 'schemas/services/example/components/schemas/private.yaml').is_file())
        (self.service / 'client-api.yaml').write_text('version: 1\nexclude: [/admin/**]\n')
        export(self.service)
        public = self.root / 'schemas/services/example'
        self.assertFalse((public / 'components/schemas/private.yaml').exists())
        self.assertEqual([op.path for op in load(self.service, public_only=True)], ['/foo/bar/show'])

    def test_same_name_alias_to_external_schema(self):
        self.data['components'] = {'schemas': {'Envelope': {
            '$ref': '../../../components/schemas/envelope.yaml#/Envelope'}}}
        self.data['paths']['/foo/bar/show'] = response('#/components/schemas/Envelope')
        self.write(self.source, self.data)
        operation, = load(self.service)
        envelope = next(model for model in operation.models if model.name == 'Envelope')
        self.assertIsNotNone(envelope.body)
        self.assertIsNone(envelope.alias)

    def test_invalid_references_are_rejected(self):
        for reference in [
            'https://example.org/spec.yaml#/Envelope',
            'file:///etc/passwd#/Envelope',
            '../../../../private.yaml#/Envelope',
            '../../../components/schemas/missing.yaml#/Missing',
            '../../../components/schemas/child.yaml#/Missing',
            '../../../components/schemas/child.yaml',
            '/absolute.yaml#/Envelope',
        ]:
            with self.subTest(reference=reference):
                self.data['paths']['/foo/bar/show'] = response(reference)
                self.write(self.source, self.data)
                with self.assertRaises(ValueError):
                    load(self.service)
                with self.assertRaises(ValueError):
                    export(self.service)

    def test_cycles_conflicts_and_unused_invalid_components(self):
        child = self.components / 'child.yaml'
        for data in [
            {'Child': {'$ref': 'envelope.yaml#/Envelope'}},
            {'Child': {'$ref': '#/Child'}},
            {'Child': {'type': 'unknown'}},
        ]:
            with self.subTest(data=data):
                self.write(child, data)
                with self.assertRaises(ValueError):
                    load(self.service)
        self.write(child, {'Child': {'type': 'string'}})
        self.write(self.components / 'unused.yaml', {'Unused': {'type': 'unknown'}})
        with self.assertRaises(ValueError):
            load(self.service)
        (self.components / 'unused.yaml').unlink()
        self.data['components'] = {'schemas': {'Child': {'type': 'integer'}}}
        self.write(self.source, self.data)
        with self.assertRaisesRegex(ValueError, 'Conflicting'):
            load(self.service)

    def test_symlink_escape_is_rejected_before_export(self):
        outside = self.root / 'private.yaml'
        outside.write_text('Private: {type: string}')
        (self.components / 'escape.yaml').symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'выходит за каталог'):
            load(self.service)
        with self.assertRaisesRegex(ValueError, 'выходит за каталог'):
            export(self.service)


    def test_invalid_document_shape_has_clear_error(self):
        for content in ['[]', 'null', 'components: []', 'components: {schemas: []}']:
            with self.subTest(content=content):
                self.source.write_text(content)
                with self.assertRaises(ValueError):
                    load(self.service)
