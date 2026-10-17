#!/usr/bin/env python3
"""Создание заготовок, проверка без записи или генерация в каталог сборки."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import yaml
from model import identifier, parse_spec
from render import config_block, outputs, view

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


def load(service):
    identifier(service.name)
    files = sorted(p for p in (service / 'docs/api').rglob('*') if p.suffix in {'.yaml', '.yml', '.json'})
    if not files:
        raise ValueError(f'{service}: no OpenAPI specs')
    ops = []
    for path in files:
        try:
            ops.extend(parse_spec(yaml.load(path.read_text(), Loader=UniqueLoader)))
        except (ValueError, KeyError, TypeError, yaml.YAMLError) as exc:
            raise ValueError(f'{path.relative_to(service)}: {exc}') from exc
    if not ops:
        raise ValueError('No HTTP operations')
    for selector in (lambda op: op.rel, lambda op: op.handler, lambda op: (op.path, op.method), lambda op: (re.sub(r'\{[^}]+\}', '{}', op.path), op.method)):
        values = [selector(op) for op in ops]
        if len(values) != len(set(values)):
            raise ValueError('Duplicate or colliding operations/paths')
    return sorted(ops, key=lambda op: op.rel)


def configuration(service, ops):
    path = service / 'configs/static_config.yaml'
    text = path.read_text()
    if text.count(BEGIN) != 1 or text.count(END) != 1 or text.index(BEGIN) > text.index(END):
        raise ValueError('Exactly one ordered pair of OpenAPI handler markers is required')
    return re.sub(r'(?m)^ *' + BEGIN + r'.*?^ *' + END, '        ' + BEGIN + '\n' + config_block(ops) + '\n        ' + END, text, flags=re.S)


def digest(service, ops):
    h = hashlib.sha256()
    for base, paths in [(service / 'docs/api', sorted(p for p in (service / 'docs/api').rglob('*') if p.suffix in {'.yaml', '.yml', '.json'})), (Path(__file__).parent, sorted(Path(__file__).parent.glob('*.py')))]:
        for p in paths:
            h.update(p.relative_to(base).as_posix().encode() + b'\0' + p.read_bytes())
    for op in ops:
        h.update(f'{op.rel}:{(service / "src/deps.hpp").exists()}'.encode())
    style = Path(__file__).resolve().parents[2] / '.clang-format'
    h.update(style.read_bytes())
    return 'sha256=' + h.hexdigest() + '\n'


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def run(service, check=False, output=None):
    ops = load(service)
    dependencies = {op.rel for op in ops if (service / 'src/deps.hpp').exists()}
    expected_config = configuration(service, ops)
    expected_lock = digest(service, ops)
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
    for op in ops:
        header, source = view(service.name, op, op.rel in dependencies)
        for name, content in [('view.hpp', header), ('view.cpp', source)]:
            path = service / 'src/views' / op.rel / name
            if not path.exists():
                write(path, content)
    write(service / 'configs/static_config.yaml', expected_config)
    write(lock, expected_lock)


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
            print('\n'.join(sorted(outputs(service.name, ops, deps))))
        else:
            run(service, args.check, args.output_dir)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        parser.exit(1, f'{args.service_dir}: {exc}\n')


if __name__ == '__main__':
    main()
