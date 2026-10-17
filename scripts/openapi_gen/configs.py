"""Типизированные конфиги, контексты и автоматически создаваемые HTTP-настройки."""
from dataclasses import replace
from pathlib import Path
import json
import re
import yaml
from model import keys, parse_body, parse_spec, enum_types, Operation
from cpp_types import declarations, enums, quoted, type_cpp
from render import fields, format_cpp

ROOT = Path(__file__).resolve().parents[2]


def root_for(service):
    return service.parent.parent if service.parent.name == 'services' else ROOT


def pascal(value):
    return ''.join(word[:1].upper() + word[1:].lower() if value.isupper() else word[:1].upper() + word[1:]
                   for word in value.split('_'))


def http_name(service):
    return service.upper() + '_HTTP_CLIENT_CONFIG'


def automatic(root, loader):
    result = {}
    for service in sorted((root / 'services').iterdir()):
        if not (service / 'service.yaml').is_file():
            continue
        paths = []
        for p in sorted((root / 'schemas/services' / service.name).rglob('*')):
            if p.suffix in {'.yaml', '.yml', '.json'}:
                paths.extend(op.path for op in parse_spec(yaml.load(p.read_text(), Loader=loader)))
        if paths:
            name = http_name(service.name)
            result[f'{service.name}/{name}'] = {
                'name': name, 'mode': 'http-client', 'generated': True,
                'paths': sorted(paths), 'default': {'**default**': {'timeout': 1000, 'max_retries': 0}},
            }
    return result


def auto_files(service, loader):
    root = root_for(service)
    key = f'{service.name}/{http_name(service.name)}'
    spec = automatic(root, loader).get(key)
    return {root / 'schemas/configs' / (key + '.yaml'): yaml.safe_dump(spec, sort_keys=False)} if spec else {}


def catalog(root, loader):
    contexts = {}
    names = set()
    for p in sorted((root / 'schemas/config_contexts').rglob('*.yaml')):
        spec = yaml.load(p.read_text(), Loader=loader)
        keys(spec, {'name', 'schema'}, str(p))
        validate_name(spec['name'], p)
        if spec['name'] in names:
            raise ValueError(f'Duplicate config context name: {spec["name"]}')
        names.add(spec['name'])
        validate_object(spec['schema'])
        parse_body(spec['schema'], {})
        contexts[p.relative_to(root / 'schemas/config_contexts').with_suffix('').as_posix()] = spec
    configs = automatic(root, loader)
    for p in sorted((root / 'schemas/configs').rglob('*.yaml')):
        spec = yaml.load(p.read_text(), Loader=loader)
        path = p.relative_to(root / 'schemas/configs').with_suffix('').as_posix()
        if isinstance(spec, dict) and spec.get('generated') is True:
            if path not in configs:
                raise ValueError(f'Unknown generated config schema: {p}')
            continue
        keys(spec, {'name', 'mode', 'context', 'schema', 'default'}, str(p))
        validate_name(spec['name'], p)
        if path in configs:
            raise ValueError(f'Config schema collides with automatic HTTP config: {p}')
        if spec.get('mode') not in {'plain', 'conditional'}:
            raise ValueError(f'{p}: mode must be plain or conditional')
        validate_object(spec['schema'])
        parse_body(spec['schema'], {})
        if spec['mode'] == 'conditional':
            if spec.get('context') not in contexts:
                raise ValueError(f'{p}: unknown config context')
        elif 'context' in spec:
            raise ValueError(f'{p}: plain configs cannot have a context')
        if 'default' not in spec:
            raise ValueError(f'{p}: config startup default is required')
        validate_default(spec)
        configs[path] = spec
    names = set()
    for spec in configs.values():
        if spec['name'] in names:
            raise ValueError(f'Duplicate global config name: {spec["name"]}')
        names.add(spec['name'])
    return contexts, configs


def validate_object(schema):
    if not isinstance(schema, dict):
        raise ValueError('Config schema must be an object')
    if schema.get('additionalProperties', False) is not False:
        raise ValueError('Config objects cannot allow undeclared properties')
    schema['additionalProperties'] = False
    body = parse_body(schema, {})
    if len({pascal(f.name) for f in body.fields}) != len(body.fields):
        raise ValueError('Config fields collide after PascalCase conversion')


def validate_name(name, path):
    if not isinstance(name, str) or not re.fullmatch(r'[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*', name) or path.stem != name:
        raise ValueError(f'{path}: expected matching UPPER_CASE name and filename')


def validate_default(spec):
    from validation import validate_value
    body = parse_body(spec['schema'], {})
    value = spec['default']
    if not isinstance(value, dict) or set(value) - {f.name for f in body.fields}:
        raise ValueError(f'{spec["name"]}: invalid default object')
    for f in body.fields:
        if f.required and f.name not in value:
            raise ValueError(f'{spec["name"]}: missing required default field {f.name}')
        if f.name in value:
            validate_value(value[f.name], f.type.schema)


def selected(service, loader):
    path = service / 'service.yaml'
    spec = yaml.load(path.read_text(), Loader=loader) if path.exists() else {}
    contexts, configs = catalog(root_for(service), loader)
    entries = spec.get('configs', [])
    if not isinstance(entries, list) or any(not isinstance(item, str) for item in entries) or len(entries) != len(set(entries)):
        raise ValueError('service.yaml: configs must be a list of unique global config names')
    by_name = {spec['name']: path for path, spec in configs.items()}
    for item in entries:
        if item not in by_name:
            raise ValueError(f'service.yaml: unknown config {item}')
    entries = [by_name[name] for name in entries]
    from clients import client_specs
    for _, provider in client_specs(service, loader):
        ref = f'{provider.name}/{http_name(provider.name)}'
        if ref not in configs:
            raise ValueError(f'No public HTTP config for {provider.name}')
        if ref not in entries:
            entries.append(ref)
    if service.name == 'configs':
        entries = list(configs)
    return contexts, configs, entries


def body_code(spec, ns, context=False):
    body = parse_body(spec['schema'], {})
    members = tuple(replace(f, name=pascal(f.name)) for f in body.fields)
    if len({f.name for f in members}) != len(members):
        raise ValueError(f'{spec["name"]}: field names collide after PascalCase conversion')
    # Enum остаются в собственном namespace, поля доступны в PascalCase.
    op = Operation('/', 'GET', '', (), body, ())
    types = enum_types(op)
    if any(typ.enum_name in {'Value', 'ParseValue', 'ValidatePredicate', 'ToJson'} for typ in types):
        raise ValueError(f'{spec["name"]}: enum name is reserved')
    text = ''.join(enums(typ) for typ in types)
    text += declarations('Value', members) + f'struct Value {{\n{fields(members, "::" + ns)}\n}};\n'
    if context:
        text += 'inline platform::configs::Json ToJson(const Value& value) {\nuserver::formats::json::ValueBuilder json(userver::formats::json::Type::kObject);\n'
        for raw, f in zip(body.fields, members):
            access = f'*value.{f.name}' if f.optional else f'value.{f.name}'
            line = f'Validate_Value_{f.name}({access}); json[{quoted(raw.name)}] = {access};'
            text += f'if (value.{f.name}) {{ {line} }}\n' if f.optional else line + '\n'
        text += 'return json.ExtractValue();\n}\n'
        text += 'inline void ValidatePredicate(platform::configs::Predicate& predicate) {\n'
        for raw, f in zip(body.fields, members):
            typ = type_cpp(f.type, '::' + ns)
            text += f'if (predicate.Field == {quoted(raw.name)}) {{\n'
            text += 'if (predicate.Type == "exists") { if (!predicate.Value.IsBool()) throw std::invalid_argument("exists expects boolean"); return; }\n'
            allowed = ['eq', 'in'] + (['lt', 'lte', 'gt', 'gte'] if f.type.kind in {'integer', 'number'} and not f.type.enum_name else [])
            text += 'if (' + ' && '.join(f'predicate.Type != {quoted(op)}' for op in allowed) + ') throw std::invalid_argument("Unsupported predicate type");\n'
            text += f'auto validate = [&](const auto& json) {{ const auto value = broker_codegen::ParseJson<{typ}>(json); Validate_Value_{f.name}(value); return userver::formats::json::ValueBuilder(value).ExtractValue(); }};\n'
            text += 'if (predicate.Type == "in") { if (!predicate.Value.IsArray() || predicate.Value.GetSize() == 0) throw std::invalid_argument("in expects nonempty array"); userver::formats::json::ValueBuilder normalized(userver::formats::json::Type::kArray); for (const auto& item : predicate.Value) normalized.PushBack(validate(item)); predicate.Value = normalized.ExtractValue(); } else predicate.Value = validate(predicate.Value);\nreturn;\n}\n'
        text += 'throw std::invalid_argument("Unknown predicate field");\n}\n'
    else:
        text += 'inline Value ParseValue(const platform::configs::Json& json) {\n'
        text += 'platform::configs::ObjectKeys(json, {' + ', '.join(quoted(f.name) for f in body.fields) + '});\nValue value;\n'
        for raw, f in zip(body.fields, members):
            key = quoted(raw.name)
            if f.required:
                text += f'if (!json.HasMember({key})) throw std::invalid_argument("Missing required config field");\n'
            text += f'if (json.HasMember({key})) {{ auto item = broker_codegen::ParseJson<{f.type.cpp}>(json[{key}]); Validate_Value_{f.name}(item); value.{f.name} = std::move(item); }}\n'
        text += 'return value;\n}\n'
    return text


def outputs(service, loader):
    contexts, configs, entries = selected(service, loader)
    if not entries:
        return {}
    text = '#pragma once\n#include <openapi/runtime.hpp>\n#include <platform/configs/runtime.hpp>\n'
    used_contexts = sorted({configs[entry]['context'] for entry in entries if configs[entry]['mode'] == 'conditional'})
    for context in used_contexts:
        spec = contexts[context]
        ns = 'config_contexts::detail_' + spec['name']
        text += f'namespace {ns} {{\n' + body_code(spec, ns, True) + f'}}  // namespace {ns}\n'
        text += f'namespace config_contexts {{ using {pascal(spec["name"])} = {ns}::Value; }}\n'
    for entry in entries:
        spec = configs[entry];name = spec['name'];ns = 'configs::detail_' + name
        text += f'namespace {ns} {{\n'
        if spec['mode'] == 'http-client':
            typ = 'platform::configs::HttpClientConfig'
            text += 'using Value = platform::configs::HttpClientConfig;\ninline Value ParseValue(const platform::configs::Json& json) { return platform::configs::ParseHttpClient(json, {' + ', '.join(quoted(path) for path in spec['paths']) + '}); }\n'
            parser = 'ParseValue';default = spec['default']
        else:
            text += body_code(spec, ns)
            typ = ns + '::Value';parser = 'ParseValue';default = spec['default']
            if spec['mode'] == 'conditional':
                context = contexts[spec['context']];context_ns = 'config_contexts::detail_' + context['name'];context_typ = 'config_contexts::' + pascal(context['name'])
                text += f'inline platform::configs::Conditional<Value, {context_typ}> ParseConfig(const platform::configs::Json& json) {{ return platform::configs::ParseConditional<Value, {context_typ}>(json, ParseValue, {context_ns}::ValidatePredicate); }}\n'
                parser = 'ParseConfig';default = {'clauses': [], 'default': default}
        text += f'}}  // namespace {ns}\n'
        key = f'platform::configs::ConditionalKey<{typ}, {context_typ}>' if spec['mode'] == 'conditional' else f'platform::configs::PlainKey<{typ}>'
        text += f'namespace configs {{ inline const {key} {name}{{{{{quoted(name)}, {ns}::{parser}, userver::dynamic_config::DefaultAsJsonString{{{quoted(json.dumps(default, ensure_ascii=False))}}}}}}}; }}\n'
    if service.name == 'configs':
        text += 'namespace configs {\ninline platform::configs::Json Defaults() { userver::formats::json::ValueBuilder out(userver::formats::json::Type::kObject);\n'
        for entry in entries:
            spec = configs[entry];default = spec['default'] if spec['mode'] != 'conditional' else {'clauses': [], 'default': spec['default']}
            text += f'out[{quoted(spec["name"])}] = userver::formats::json::FromString({quoted(json.dumps(default, ensure_ascii=False))});\n'
        text += 'return out.ExtractValue();\n}\ninline void Validate(const platform::configs::Json& values) {\nplatform::configs::ObjectKeys(values, {' + ', '.join(quoted(configs[e]['name']) for e in entries) + '});\n'
        for entry in entries:
            spec = configs[entry];parser = 'ParseConfig' if spec['mode'] == 'conditional' else 'ParseValue'
            text += f'if (values.HasMember({quoted(spec["name"])})) (void)detail_{spec["name"]}::{parser}(values[{quoted(spec["name"])}]);\n'
        text += '}\n}  // namespace configs\n'
    return {'include/configs/keys.hpp': format_cpp(text)}
