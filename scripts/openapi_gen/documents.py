"""Локальные OpenAPI-документы и ссылки на общие схемы без сетевого доступа."""
from copy import deepcopy
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit
import posixpath

import yaml
from model import identifier, parse_spec, validate_operations

EXTENSIONS = {'.yaml', '.yml', '.json'}


def read_files(roots, boundary):
    boundary = boundary.resolve()
    result = {}
    for root in roots:
        for path in sorted(root.rglob('*')):
            if path.suffix not in EXTENSIONS:
                continue
            if not path.resolve().is_relative_to(boundary):
                raise ValueError(f'Файл выходит за каталог контрактов: {path}')
            result[path] = path.read_text()
    return result


class Documents:
    def __init__(self, files, root, components, loader):
        self.root = root.resolve()
        self.components = components.resolve()
        self.files = {}
        self.cache = {}
        self.loader = loader
        for path, text in files.items():
            path = path.resolve()
            if not path.is_relative_to(self.root):
                raise ValueError(f'Файл выходит за каталог контрактов: {path}')
            self.files[path] = text

    def document(self, path):
        path = path.resolve()
        if path not in self.files:
            raise ValueError(f'Файл контракта не найден: {path}')
        if path not in self.cache:
            value = yaml.load(self.files[path], Loader=self.loader)
            if not isinstance(value, dict):
                raise ValueError(f'{path}: ожидался YAML/JSON-объект')
            self.cache[path] = value
        return self.cache[path]

    def target(self, origin, ref):
        if not isinstance(ref, str) or '\\' in ref:
            raise ValueError(f'Неверная ссылка: {ref!r}')
        uri = urlsplit(ref)
        if uri.scheme or uri.netloc or uri.query or uri.path.startswith('/'):
            raise ValueError(f'Разрешены только локальные относительные ссылки: {ref}')
        target = (origin.parent / unquote(uri.path)).resolve() if uri.path else origin.resolve()
        if not target.is_relative_to(self.root):
            raise ValueError(f'Ссылка выходит за каталог контрактов: {ref}')
        if target != origin.resolve() and not target.is_relative_to(self.components):
            raise ValueError(f'Общие схемы между файлами нужно вынести в docs/components: {ref}')
        pointer = unquote(uri.fragment)
        if not pointer.startswith('/'):
            raise ValueError(f'Ссылка должна содержать JSON Pointer на именованную схему: {ref}')
        tokens = [part.replace('~1', '/').replace('~0', '~') for part in pointer[1:].split('/')]
        identifier(tokens[-1])
        value = self.document(target)
        for token in tokens:
            if not isinstance(value, dict) or token not in value:
                raise ValueError(f'Не найдена схема {target}#{pointer}')
            value = value[token]
        if not isinstance(value, dict):
            raise ValueError(f'Ожидалась схема-объект: {target}#{pointer}')
        return target, pointer, tokens[-1], value

    def bundle(self, path, data=None, model_refs=None):
        path = path.resolve()
        data = deepcopy(self.document(path) if data is None else data)
        models, owners, imported = {}, {}, {}

        def ensure(origin, ref):
            target, pointer, name, schema = self.target(origin, ref)
            # Локальный Foo: {$ref: 'file.yaml#/Foo'} не создает alias Foo -> Foo.
            aliases = set()
            while set(schema) == {'$ref'}:
                next_target = self.target(target, schema['$ref'])
                if next_target[2] != name:
                    break
                key = (target, pointer)
                if key in aliases:
                    raise ValueError(f'Recursive schema reference: {target}#{pointer}')
                aliases.add(key)
                target, pointer, name, schema = next_target
            key = (target, pointer)
            if key in imported:
                return {'$ref': '#/components/schemas/' + name}
            imported[key] = name
            if name in models:
                if models[name] is None or models[name] != walk(schema, target):
                    raise ValueError(f'Conflicting components.schemas model: {name} ({owners[name]}, {target})')
            else:
                owners[name] = target
                models[name] = None
                models[name] = walk(schema, target)
            return {'$ref': '#/components/schemas/' + name}

        def walk(value, origin):
            if isinstance(value, list):
                return [walk(item, origin) for item in value]
            if not isinstance(value, dict):
                return value
            if '$ref' in value:
                if set(value) != {'$ref'}:
                    raise ValueError('У $ref не должно быть соседних свойств')
                return ensure(origin, value['$ref'])
            return {key: walk(item, origin) for key, item in value.items()}

        components = data.get('components', {})
        if not isinstance(components, dict) or not isinstance(components.get('schemas', {}), dict):
            raise ValueError(f'{path}: components и components.schemas должны быть объектами')
        original = components.get('schemas', {})
        for name in original:
            ensure(path, '#/components/schemas/' + name)
        for ref in (model_refs or {}).values():
            ensure(path, ref)
        # Остальные секции components сохраняются: парсер явно отклонит неподдерживаемые.
        if 'components' in data:
            data['components'].pop('schemas', None)
        data = walk(data, path)
        if models or 'components' in data:
            data.setdefault('components', {})['schemas'] = models
        return data

    def validate_components(self):
        for path in sorted(self.files):
            if not path.is_relative_to(self.components):
                continue
            data = self.document(path)
            if not isinstance(data, dict):
                raise ValueError(f'{path}: ожидается набор именованных схем')
            for name in data:
                identifier(name)
            bundled = self.bundle(path, {'openapi': '3.0.3', 'paths': {}},
                                  {name: '#/' + name for name in data})
            parse_spec(bundled)

    def operations(self, api):
        self.validate_components()
        result = []
        for path in sorted(self.files):
            if path.is_relative_to(api.resolve()) and not path.is_relative_to(self.components):
                try:
                    result.extend(parse_spec(self.bundle(path)))
                except (ValueError, KeyError, TypeError, yaml.YAMLError) as error:
                    raise ValueError(f'{path}: {error}') from error
        validate_operations(result)
        return sorted(result, key=lambda op: op.rel)

    def dependencies(self, data, path):
        visited = {path.resolve()}
        def visit(value, origin):
            if isinstance(value, list):
                for item in value:
                    visit(item, origin)
            elif isinstance(value, dict):
                if '$ref' in value:
                    target, _, _, _ = self.target(origin, value['$ref'])
                    if target not in visited:
                        visited.add(target)
                        visit(self.document(target), target)
                else:
                    for item in value.values():
                        visit(item, origin)
        visit(data, path.resolve())
        return visited - {path.resolve()}

    def rewrite(self, data, path, destination):
        """Сохраняем ссылки после удаления уровня docs/api при клиентском экспорте."""
        def visit(value):
            if isinstance(value, list):
                return [visit(item) for item in value]
            if not isinstance(value, dict):
                return value
            if '$ref' in value:
                target, pointer, _, _ = self.target(path, value['$ref'])
                relative = '' if target == path.resolve() else posixpath.relpath(
                    destination(target).as_posix(), destination(path).parent.as_posix())
                return {'$ref': quote(relative, safe='/.-_') + '#' + quote(pointer, safe='/~')}
            return {key: visit(item) for key, item in value.items()}
        return visit(data)
