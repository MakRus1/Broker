#!/usr/bin/env python3
"""Создание заготовок, проверка без записи или генерация в каталог сборки."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import yaml
from model import identifier, parse_spec, validate_operations
from render import config_block, outputs, view
from features import prepare
from clients import client_outputs, client_specs
import configs

class UniqueLoader(yaml.SafeLoader):
    """Повторяющиеся ключи YAML считаются ошибкой контракта."""


def unique_mapping(loader, node, deep=False):
    pairs = loader.construct_pairs(node, deep=deep)
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'Повторяющийся ключ YAML: {key}')
        result[key] = value
    return result


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)

BEGIN = '# OPENAPI_HANDLERS_BEGIN'
END = '# OPENAPI_HANDLERS_END'


def public_schema_dir(service):
    root = service.parent.parent if service.parent.name == 'services' else Path(__file__).resolve().parents[2]
    return root / 'schemas/services' / service.name


def specification_files(service, additions=None, public_only=False):
    roots = [public_schema_dir(service)] + ([] if public_only else [service / 'docs/api'])
    files = {p: p.read_text() for base in roots for p in base.rglob('*')
             if p.suffix in {'.yaml', '.yml', '.json'}}
    files.update({p: text for p, text in (additions or {}).items()
                  if p.is_relative_to(service / 'docs/api') and p.suffix in {'.yaml', '.yml', '.json'}})
    return files


def load(service, additions=None, public_only=False):
    identifier(service.name)
    files = specification_files(service, additions, public_only)
    if not files:
        raise ValueError(f'{service}: no OpenAPI specs')
    ops = []
    for path, content in sorted(files.items()):
        try:
            ops.extend(parse_spec(yaml.load(content, Loader=UniqueLoader)))
        except (ValueError, KeyError, TypeError, yaml.YAMLError) as exc:
            raise ValueError(f'{path}: {exc}') from exc
    if not ops:
        raise ValueError('No HTTP operations')
    validate_operations(ops)
    return sorted(ops, key=lambda op: op.rel)


def configuration(service, ops, source=None):
    path = service / 'configs/static_config.yaml'
    text = path.read_text() if source is None else source
    if text.count(BEGIN) != 1 or text.count(END) != 1 or text.index(BEGIN) > text.index(END):
        raise ValueError('Exactly one ordered pair of OpenAPI handler markers is required')
    return re.sub(r'(?m)^ *' + BEGIN + r'.*?^ *' + END, '        ' + BEGIN + '\n' + config_block(ops) + '\n        ' + END, text, flags=re.S)


def digest(service, ops, additions=None):
    h = hashlib.sha256()
    for p, content in sorted(specification_files(service, additions).items()):
        h.update((('public/' + p.relative_to(public_schema_dir(service)).as_posix()) if p.is_relative_to(public_schema_dir(service)) else ('internal/' + p.relative_to(service / 'docs/api').as_posix())).encode() + b'\0' + content.encode())
    for alias, provider in client_specs(service, UniqueLoader):
        h.update(alias.encode())
        for p, content in sorted(specification_files(provider, public_only=True).items()):
            h.update(p.relative_to(public_schema_dir(provider)).as_posix().encode() + b"\0" + content.encode())
    contexts, catalog, entries = configs.selected(service, UniqueLoader)
    for entry in sorted(entries):
        spec = catalog[entry]
        h.update(json.dumps(spec, sort_keys=True).encode())
        if spec['mode'] == 'conditional':
            h.update(json.dumps(contexts[spec['context']], sort_keys=True).encode())
    base = Path(__file__).parent
    for p in sorted(base.glob('*.py')):
        h.update(p.relative_to(base).as_posix().encode() + b'\0' + p.read_bytes())
    for op in ops:
        h.update(f'{op.rel}:{(service / "src/deps.hpp").exists()}'.encode())
    if (service / "service.yaml").exists():
        h.update((service / "service.yaml").read_bytes())
    style = Path(__file__).resolve().parents[2] / '.clang-format'
    h.update(style.read_bytes())
    return 'sha256=' + h.hexdigest() + '\n'


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def run(service, check=False, output=None):
    feature_files = prepare(service, UniqueLoader)
    feature_files.update(configs.auto_files(service, UniqueLoader))
    stale = [str(path) for path, content in feature_files.items()
             if not path.exists() or path.read_text() != content]
    if check or output is not None:
        if stale:
            raise ValueError('Обвязка service.yaml устарела; выполните make gen: ' + ', '.join(stale))
    ops = load(service, feature_files)
    generated_clients = client_outputs(service, UniqueLoader, load)
    generated_clients.update(configs.outputs(service, UniqueLoader))
    dependencies = {op.rel for op in ops if (service / 'src/deps.hpp').exists()}
    config_path = service / 'configs/static_config.yaml'
    expected_config = configuration(service, ops, feature_files.get(config_path))
    expected_lock = digest(service, ops, feature_files)
    lock = service / 'docs/api/codegen.lock'
    if check:
        errors = []
        if not lock.exists() or lock.read_text() != expected_lock:
            errors.append('codegen.lock is stale; run make gen')
        if (service / 'configs/static_config.yaml').read_text() != expected_config:
            errors.append('static_config.yaml handler block is stale; run make gen')
        for op in ops:
            for name in ('view.hpp', 'view.cpp'):
                if not (service / 'src/views' / op.rel / name).is_file():
                    errors.append(f'Missing src/views/{op.rel}/{name}; run make gen')
        if errors:
            raise ValueError('\n'.join(errors))
        return
    if output is not None:
        generated = outputs(service.name, ops, dependencies)
        generated.update(generated_clients)
        # Удаляем только файлы, ранее созданные этим генератором.
        manifest = output / 'manifest.json'
        previous = json.loads(manifest.read_text()) if manifest.exists() else []
        for name in set(previous) - set(generated):
            target = (output / name).resolve()
            if not target.is_relative_to(output.resolve()):
                raise ValueError('Invalid generated manifest path')
            target.unlink(missing_ok=True)
        for name, text in generated.items():
            write(output / name, text)
        write(manifest, json.dumps(sorted(generated), indent=2) + '\n')
        return
    planned = dict(feature_files)
    for op in ops:
        header, source = view(service.name, op, op.rel in dependencies)
        for name, content in [('view.hpp', header), ('view.cpp', source)]:
            path = service / 'src/views' / op.rel / name
            if not path.exists() and path not in planned:
                planned[path] = content
    planned[config_path] = expected_config
    planned[lock] = expected_lock
    changed = {path: content for path, content in planned.items()
               if not path.exists() or path.read_text() != content}
    for path in changed:
        if not path.resolve().is_relative_to(service.resolve()) and path not in configs.auto_files(service, UniqueLoader):
            raise ValueError(f'Выход за каталог сервиса запрещен: {path}')
    before = {path: path.read_bytes() if path.exists() else None for path in changed}
    try:
        for path, content in changed.items():
            write(path, content)
    except BaseException:
        for path, content in before.items():
            if content is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(content)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service-dir', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--output-dir', type=Path)
    mode.add_argument('--list-outputs', action='store_true')
    args = parser.parse_args()
    try:
        service = args.service_dir.resolve()
        if args.output_dir and (args.output_dir.resolve() == service or args.output_dir.resolve().is_relative_to(service)):
            raise ValueError('Build output must be outside the service source directory')
        if args.list_outputs:
            ops = load(service)
            deps = {op.rel for op in ops if (service / 'src/deps.hpp').exists()}
            generated = outputs(service.name, ops, deps)
            generated.update(client_outputs(service, UniqueLoader, load))
            generated.update(configs.outputs(service, UniqueLoader))
            print('\n'.join(sorted(generated)))
        else:
            run(service, args.check, args.output_dir)
    except (ValueError, OSError, KeyError, TypeError, yaml.YAMLError) as exc:
        parser.exit(1, f'{args.service_dir}: {exc}\n')


if __name__ == '__main__':
    main()
