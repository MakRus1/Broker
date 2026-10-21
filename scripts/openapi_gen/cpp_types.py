"""Типизированные enum, значения default и проверки ограничений C++."""
from dataclasses import replace
import json
import re
from model import CPP_KEYWORDS


def quoted(value):
    text = json.dumps(value, ensure_ascii=False)
    return re.sub(r'\\u00([0-1][0-9a-f])', lambda m: '\\' + format(int(m[1], 16), '03o'), text)


def enum_labels(typ):
    values = typ.enum_values or tuple(typ.schema['enum'])
    mapping = {}
    for value in values:
        words = re.findall(r'[a-zA-Z0-9]+', str(value))
        label = ''.join(word[:1].upper() + word[1:] for word in words)
        if typ.kind != 'string':
            label = 'Value' + ('Minus' if str(value).startswith('-') else '') + label
        if not label or label[0].isdigit() or label in CPP_KEYWORDS:
            raise ValueError(f'Enum {typ.enum_name}: значение {value!r} не образует допустимое C++-имя')
        if label in mapping.values():
            raise ValueError(f'Enum {typ.enum_name}: конфликт C++-имени {label!r} для значения {value!r}')
        mapping[value] = label
    return [mapping[value] for value in typ.schema['enum']]


def type_cpp(typ, scope=''):
    if typ.nullable:
        return 'std::optional<' + type_cpp(replace(typ, nullable=False), scope) + '>'
    if typ.model_name:
        return typ.cpp
    if typ.enum_name:
        return scope + '::' + typ.enum_name if scope else typ.enum_name
    if typ.item:
        return 'std::vector<' + type_cpp(typ.item, scope) + '>'
    return typ.cpp


def literal(typ, value, scope=''):
    if typ.nullable:
        if value is None:
            return 'std::nullopt'
        return type_cpp(typ, scope) + '{' + literal(replace(typ, nullable=False), value, scope) + '}'
    if typ.enum_name:
        return f'{type_cpp(typ, scope)}::{enum_labels(typ)[typ.schema["enum"].index(value)]}'
    if typ.kind == 'array':
        return type_cpp(typ, scope) + '{' + ', '.join(literal(typ.item, item, scope) for item in value) + '}'
    if typ.kind == 'string':
        return 'std::string{' + quoted(value) + (f', {len(value.encode("utf-8"))}' if '\0' in value else '') + '}'
    if typ.kind == 'boolean':
        return 'true' if value else 'false'
    if value == -(2**63) and typ.kind == 'integer':
        return 'std::numeric_limits<std::int64_t>::min()'
    return f'static_cast<{typ.cpp}>({json.dumps(value)})'


def enums(typ):
    name = typ.cpp
    base = replace(typ, enum_name=None)
    values = typ.schema['enum']
    labels = enum_labels(typ)
    underlying = f' : {base.cpp}' if typ.kind in {'integer', 'boolean'} else ''
    members = [f'{label} = {literal(base, value)}' if underlying else label for label, value in zip(labels, values)]
    code = f'enum class {name}{underlying} {{' + ', '.join(members) + '};\n'
    code += f'inline {base.cpp} EnumValue({name} value) {{\nswitch (value) {{\n'
    code += ''.join(f'case {name}::{label}: return {literal(base, value)};\n' for label, value in zip(labels, values))
    code += '}\nthrow std::invalid_argument("enum");\n}\n'
    code += f'namespace detail {{\ninline {name} ParseEnum(const {base.cpp}& value, {name}) {{\n'
    code += ''.join(f'if (value == {literal(base, value)}) return {name}::{label};\n' for label, value in zip(labels, values))
    code += 'throw std::invalid_argument("enum");\n}\n}  // namespace detail\n'
    code += f'''inline {name} ParseEnumScalar(const std::string& text, {name}) {{
return detail::ParseEnum(broker_codegen::ParseScalar<{base.cpp}>(text), {name}{{}});
}}
inline {name} ParseEnumJson(const userver::formats::json::Value& value, {name}) {{
return detail::ParseEnum(broker_codegen::ParseJson<{base.cpp}>(value), {name}{{}});
}}
inline userver::formats::json::Value Serialize({name} value,
    userver::formats::serialize::To<userver::formats::json::Value>) {{
return userver::formats::json::ValueBuilder(EnumValue(value)).ExtractValue();
}}
'''
    return code


def validator(typ, name):
    if typ.nullable:
        return validator(replace(typ, nullable=False), name + '_value') + f'inline void Validate_{name}(const {typ.cpp}& value) {{ if (value.has_value()) Validate_{name}_value(value.value()); }}\n'
    before = validator(typ.item, name + '_item') if typ.item else ''
    raw = 'EnumValue(value)' if typ.enum_name else 'value'
    schema = typ.schema
    checks = []
    if typ.kind == 'record':
        checks.append('ValidateModel(value);')
    if typ.kind == 'object':
        checks.append('if (!value.IsObject()) throw std::invalid_argument("object");')
    if typ.enum_name:
        allowed = " && ".join(f"value != {literal(typ, value)}" for value in typ.schema["enum"])
        checks.append(f'if ({allowed}) throw std::invalid_argument("enum");')
    if typ.kind == 'number':
        checks.append(f'if (!std::isfinite({raw})) throw std::invalid_argument("number");')
    if typ.kind == 'string' and not ('minLength' in schema or 'maxLength' in schema):
        checks.append(f'(void)broker_codegen::Utf8Length({raw});')
    if typ.kind == 'array':
        checks.append(f'for (const auto& item : value) Validate_{name}_item(item);')
        if schema.get('uniqueItems'):
            checks.append('broker_codegen::RequireUnique(value);')
    for key, operation in [('minimum', '<=' if schema.get('exclusiveMinimum') else '<'),
                           ('maximum', '>=' if schema.get('exclusiveMaximum') else '>')]:
        if key in schema:
            bound = f'static_cast<float>({json.dumps(schema[key])})' if schema.get('format') == 'float' else json.dumps(schema[key]) + 'L'
            checks.append(f'if (static_cast<long double>({raw}) {operation} {bound}) throw std::invalid_argument("{key}");')
    if 'multipleOf' in schema:
        checks.append(f'broker_codegen::CheckMultiple({raw}, {json.dumps(schema["multipleOf"])}, {"1e-6L" if typ.schema.get("format") == "float" else "1e-9L"});')
    if typ.kind == 'string' and ('minLength' in schema or 'maxLength' in schema):
        checks.append(f'const auto length = broker_codegen::Utf8Length({raw});')
    for key, expression, operation in [('minLength', 'length', '<'), ('maxLength', 'length', '>'),
                                       ('minItems', 'value.size()', '<'), ('maxItems', 'value.size()', '>')]:
        if key in schema:
            checks.append(f'if ({expression} {operation} {schema[key]}) throw std::invalid_argument("{key}");')
    if 'pattern' in schema:
        checks.append(f'static const userver::utils::regex pattern{{{quoted(schema["pattern"])}}}; if (!userver::utils::regex_search({raw}, pattern)) throw std::invalid_argument("pattern");')
    if typ.kind == 'string' and schema.get('format') not in {None, 'password'}:
        checks.append(f'broker_codegen::CheckFormat({raw}, {quoted(schema["format"])});')
    parameter = ' value' if checks else ''
    return before + f'inline void Validate_{name}(const {typ.cpp}&{parameter}) {{\n' + '\n'.join(checks) + '\n}\n'


def declarations(name, fields):
    return ''.join(validator(f.type, name + '_' + f.name) for f in fields)
