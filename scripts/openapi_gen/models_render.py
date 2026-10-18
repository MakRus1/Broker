"""Общие именованные модели, проверка полей и JSON-преобразования на границе HTTP."""
from dataclasses import replace
from cpp_types import declarations, enums, quoted


def bind_type(typ, namespace):
    return replace(typ,
                   model_name=f'::{namespace}::{typ.model_name}' if typ.model_name else None,
                   item=bind_type(typ.item, namespace) if typ.item else None)


def bind_body(body, namespace):
    return replace(body, fields=tuple(replace(f, type=bind_type(f.type, namespace)) for f in body.fields)) if body else None


def bind_operation(op, namespace):
    return replace(op, params=tuple(replace(f, type=bind_type(f.type, namespace)) for f in op.params),
                   body=bind_body(op.body, namespace),
                   responses=tuple(replace(r, body=bind_body(r.body, namespace)) for r in op.responses))


def render_models(models, namespace, header=True):
    from render import struct
    code = '#pragma once\n' if header else ''
    code += '''#include <openapi/runtime.hpp>
#include <optional>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
'''
    code += f'namespace {namespace} {{\n'
    for model in models:
        name = model.name
        if model.alias:
            code += f'using {name} = ::{namespace}::{model.alias};\n'
        elif model.body:
            body = bind_body(model.body, namespace)
            code += struct(name, body.fields, input_metadata=True) + '\n'
            code += f'inline void ValidateModel(const {name}& value) {{\n'
            for f in body.fields:
                call = f'Validate_{name}_{f.name}(' + ('*' if f.optional else '') + f'value.{f.name});'
                code += f'if (value.{f.name}) {{ {call} }}\n' if f.optional else call + '\n'
            code += '}\n'
            code += f'''inline {name} ParseModelJson(const userver::formats::json::Value& json,
    std::type_identity<{name}>) {{
if (!json.IsObject()) throw std::invalid_argument("object");
{name} value;
'''
            if body.strict:
                allowed = ' && '.join(f'it.GetName() != {quoted(f.name)}' for f in body.fields) or 'true'
                code += f'for (auto it = json.begin(); it != json.end(); ++it) if ({allowed}) throw std::invalid_argument("unknown field");\n'
            for index, f in enumerate(body.fields):
                key = quoted(f.name)
                if f.required:
                    code += f'if (!json.HasMember({key})) throw std::invalid_argument("missing field");\n'
                assignment = f'value.{f.name}.emplace(broker_codegen::ParseJson<{f.type.cpp}>(json[{key}]))' if f.optional else f'value.{f.name} = broker_codegen::ParseJson<{f.type.cpp}>(json[{key}])'
                code += f'if (json.HasMember({key})) {{ {assignment}; value.MarkProvided({index}); }}\n'
            code += 'ValidateModel(value);\nreturn value;\n}\n'
            code += f'''inline userver::formats::json::Value Serialize(const {name}& value,
    userver::formats::serialize::To<userver::formats::json::Value>) {{
ValidateModel(value);
userver::formats::json::ValueBuilder json(userver::formats::json::Type::kObject);
'''
            for f in body.fields:
                assignment = f'json[{quoted(f.name)}] = ' + ('*' if f.optional else '') + f'value.{f.name};'
                code += f'if (value.{f.name}) {{ {assignment} }}\n' if f.optional else assignment + '\n'
            code += 'return json.ExtractValue();\n}\n'
        elif model.type.enum_name:
            code += enums(replace(model.type, enum_name=name, model_name=None))
        else:
            code += f'using {name} = {bind_type(model.type, namespace).cpp};\n'
    return code + f'}}  // namespace {namespace}\n'
