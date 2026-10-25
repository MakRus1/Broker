"""C++-структуры и userver bindings для типов, созданных ручными SQL-миграциями."""

import re

from model import keys

BUILTINS = {
    'text': 'std::string',
    'date': 'userver::utils::datetime::Date',
    'boolean': 'bool',
    'smallint': 'std::int16_t',
    'integer': 'std::int32_t',
    'bigint': 'std::int64_t',
    'real': 'float',
    'double precision': 'double',
    'uuid': 'boost::uuids::uuid',
    'jsonb': 'userver::formats::json::Value',
    'timestamptz': 'userver::storages::postgres::TimePointTz',
}


def identifier(value):
    if (
        not isinstance(value, str)
        or not re.fullmatch(r'[a-z][a-z0-9]*(?:_[a-z0-9]+)*', value)
        or len(value) > 63
    ):
        raise ValueError(f'Некорректное PostgreSQL-имя: {value!r}')
    return value


def cpp_name(value):
    return ''.join(part[0].upper() + part[1:] for part in value.split('_'))


def inputs(service):
    path = service / 'postgresql/types.yaml'
    return [path] if path.exists() else []


def load(service, loader):
    import yaml

    files = inputs(service)
    if not files:
        return {}
    document = yaml.load(files[0].read_text(), Loader=loader)
    keys(document, {'version', 'types'}, str(files[0]))
    if type(document.get('version')) is not int or document['version'] != 1:
        raise ValueError('postgresql/types.yaml: ожидается version: 1')
    types = document.get('types')
    if not isinstance(types, dict) or not types:
        raise ValueError('postgresql/types.yaml: types — непустой словарь')
    cpp_names = set()
    for name, definition in types.items():
        if not isinstance(name, str) or name.count('.') != 1:
            raise ValueError('Тип PostgreSQL задается полным именем schema.type_vN')
        schema, short = name.split('.')
        identifier(schema)
        identifier(short)
        if not re.fullmatch(r'.+_v[1-9][0-9]*', short):
            raise ValueError(f'{name}: ожидается версионированное имя *_v1, *_v2 и т.д.')
        cpp = cpp_name(short)
        if cpp in cpp_names:
            raise ValueError(f'Конфликт C++-имен PostgreSQL-типов: {cpp}')
        cpp_names.add(cpp)
        keys(definition, {'fields'}, name)
        fields = definition.get('fields')
        if not isinstance(fields, list) or not fields:
            raise ValueError(f'{name}: fields — непустой упорядоченный список')
        names = set()
        for field in fields:
            keys(field, {'name', 'type', 'nullable'}, name)
            identifier(field.get('name'))
            if field['name'] in names:
                raise ValueError(f'{name}: повтор поля {field["name"]}')
            names.add(field['name'])
            if not isinstance(field.get('type'), str):
                raise ValueError(f'{name}.{field["name"]}: требуется type')
            if type(field.get('nullable', False)) is not bool:
                raise ValueError(f'{name}.{field["name"]}: nullable должен быть boolean')

    # Объявления сортируются по зависимостям, рекурсия и неизвестные типы запрещены.
    ordered, visiting = {}, set()

    def visit(name):
        if name in ordered:
            return
        if name in visiting:
            raise ValueError(f'Рекурсивный PostgreSQL-тип: {name}')
        visiting.add(name)
        for field in types[name]['fields']:
            base = field['type'].removesuffix('[]')
            if base in types:
                visit(base)
            elif base not in BUILTINS:
                raise ValueError(f'{name}.{field["name"]}: неподдерживаемый тип {field["type"]}')
        visiting.remove(name)
        ordered[name] = types[name]

    for name in sorted(types):
        visit(name)
    return ordered


def outputs(service, loader):
    types = load(service, loader)
    if not types:
        return {}
    from render import format_cpp

    manifest = service / 'service.yaml'
    if manifest.exists():
        import yaml

        features = yaml.load(manifest.read_text(), Loader=loader)
        if not features.get('postgresql', {}).get('enabled', False):
            raise ValueError('postgresql/types.yaml требует postgresql.enabled: true')
    code = '''#pragma once
// Сгенерировано из postgresql/types.yaml. SQL-миграции поддерживаются вручную.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
'''
    code += f'namespace {service.name}::pgtypes {{\n'
    for name, definition in types.items():
        code += f'struct {cpp_name(name.split(".")[1])} {{\n'
        for field in definition['fields']:
            base = field['type'].removesuffix('[]')
            cpp = BUILTINS[base] if base in BUILTINS else cpp_name(base.split('.')[1])
            if field['type'].endswith('[]'):
                cpp = f'std::vector<{cpp}>'
            if field.get('nullable', False):
                cpp = f'std::optional<{cpp}>'
            code += f'    {cpp} {cpp_name(field["name"])}{{}};\n'
        code += '};\n'
    code += f'}}  // namespace {service.name}::pgtypes\n'
    code += 'USERVER_NAMESPACE_BEGIN\nnamespace storages::postgres::io {\n'
    for name in types:
        cpp = f'::{service.name}::pgtypes::{cpp_name(name.split(".")[1])}'
        code += f'''template <> struct CppToUserPg<{cpp}> {{
    static constexpr DBTypeName postgres_name = "{name}";
}};
'''
    code += '}  // namespace storages::postgres::io\nUSERVER_NAMESPACE_END\n'
    return {'include/postgresql/types.hpp': format_cpp(code)}
