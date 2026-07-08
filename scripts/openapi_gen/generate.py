#!/usr/bin/env python3
"""Generate userver handlers and DTOs from OpenAPI specs in docs/api/."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

try:
    import yaml
except ImportError as exc:  # pragma: no cover
    print('PyYAML is required: pip install pyyaml', file=sys.stderr)
    raise SystemExit(1) from exc

HTTP_METHODS = frozenset({'get', 'post', 'put', 'patch', 'delete', 'head', 'options'})


@dataclass(frozen=True)
class Field:
    cpp_name: str
    json_name: str
    cpp_type: str
    required: bool


@dataclass
class Schema:
    name: str
    fields: list[Field] = field(default_factory=list)


@dataclass(frozen=True)
class ResponseSpec:
    status: int
    schema: Schema | None


@dataclass
class Operation:
    path: str
    method: str
    operation_id: str
    rel_path: str
    handler_name: str
    request_schema: Schema | None
    responses: list[ResponseSpec]
    query_params: list[Field]


HTTP_STATUS_NAMES: dict[int, str] = {
    400: 'kBadRequest',
    401: 'kUnauthorized',
    403: 'kForbidden',
    404: 'kNotFound',
    409: 'kConflict',
    422: 'kUnprocessableEntity',
    500: 'kInternalServerError',
    503: 'kServiceUnavailable',
}


def _pascal_case(value: str) -> str:
    parts = re.split(r'[^a-zA-Z0-9]+', value)
    return ''.join(part[:1].upper() + part[1:] for part in parts if part)


def _snake_case(value: str) -> str:
    value = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', value)
    value = value.replace('-', '_')
    return re.sub(r'[^a-zA-Z0-9_]+', '_', value).strip('_').lower()


def _path_to_rel_path(path: str) -> str:
    segments: list[str] = []
    for segment in path.strip('/').split('/'):
        if not segment:
            continue
        if segment.startswith('{') and segment.endswith('}'):
            segments.append('_' + segment[1:-1] + '_')
        else:
            segments.append(segment)
    return '/'.join(segments)


def _handler_name(path: str, method: str) -> str:
    slug = _path_to_rel_path(path).replace('/', '-')
    return f'handler-{slug}-{method.lower()}'


def _cpp_namespace(rel_path: str) -> str:
    return '::'.join(_snake_case(part) for part in rel_path.split('/'))


def _openapi_type_to_cpp(schema: dict[str, Any], schemas: dict[str, Any]) -> str:
    if '$ref' in schema:
        ref = schema['$ref']
        return _pascal_case(ref.rsplit('/', 1)[-1])

    schema_type = schema.get('type')
    if schema_type == 'string':
        return 'std::string'
    if schema_type == 'integer':
        return 'std::int64_t'
    if schema_type == 'number':
        return 'double'
    if schema_type == 'boolean':
        return 'bool'
    if schema_type == 'array':
        item_type = _openapi_type_to_cpp(schema.get('items', {}), schemas)
        return f'std::vector<{item_type}>'
    if schema_type == 'object':
        return 'formats::json::Value'
    return 'formats::json::Value'


def _resolve_ref(ref: str, schemas: dict[str, Any]) -> dict[str, Any]:
    name = ref.rsplit('/', 1)[-1]
    return schemas[name]


def _schema_from_components(name: str, schema: dict[str, Any], schemas: dict[str, Any]) -> Schema:
    if '$ref' in schema:
        schema = _resolve_ref(schema['$ref'], schemas)

    if schema.get('type') != 'object':
        return Schema(name=name)

    required = set(schema.get('required', []))
    fields: list[Field] = []
    for json_name, prop in schema.get('properties', {}).items():
        fields.append(
            Field(
                cpp_name=_snake_case(json_name),
                json_name=json_name,
                cpp_type=_openapi_type_to_cpp(prop, schemas),
                required=json_name in required,
            )
        )
    return Schema(name=_pascal_case(name), fields=fields)


def _load_specs(spec_dir: Path) -> list[Path]:
    files = sorted(spec_dir.glob('**/*'))
    result = [path for path in files if path.suffix in {'.yaml', '.yml', '.json'}]
    if not result:
        raise SystemExit(f'No OpenAPI specs found in {spec_dir}')
    return result


def _load_openapi(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding='utf-8')
    if path.suffix == '.json':
        return json.loads(text)
    return yaml.safe_load(text)


def _collect_operations(spec: dict[str, Any]) -> list[Operation]:
    schemas = spec.get('components', {}).get('schemas', {})
    operations: list[Operation] = []

    for path, path_item in spec.get('paths', {}).items():
        for method, operation in path_item.items():
            if method not in HTTP_METHODS:
                continue

            operation_id = operation.get('operationId') or _handler_name(path, method)
            rel_path = _path_to_rel_path(path)

            query_params: list[Field] = []
            for param in operation.get('parameters', []):
                if param.get('in') != 'query':
                    continue
                query_params.append(
                    Field(
                        cpp_name=_snake_case(param['name']),
                        json_name=param['name'],
                        cpp_type=_openapi_type_to_cpp(param.get('schema', {}), schemas),
                        required=bool(param.get('required', False)),
                    )
                )

            request_schema = None
            for _, body in operation.get('requestBody', {}).get('content', {}).items():
                schema = body.get('schema', {})
                if '$ref' in schema:
                    name = schema['$ref'].rsplit('/', 1)[-1]
                    request_schema = _schema_from_components(name, schema, schemas)
                break

            responses: list[ResponseSpec] = []
            for status_raw, response in operation.get('responses', {}).items():
                status = int(str(status_raw).split()[0])
                response_schema = None
                for _, body in response.get('content', {}).items():
                    schema = body.get('schema', {})
                    if '$ref' in schema:
                        name = schema['$ref'].rsplit('/', 1)[-1]
                        response_schema = _schema_from_components(name, schema, schemas)
                    break
                if response_schema:
                    responses.append(ResponseSpec(status=status, schema=response_schema))

            responses.sort(key=lambda item: item.status)

            operations.append(
                Operation(
                    path=path,
                    method=method.upper(),
                    operation_id=operation_id,
                    rel_path=rel_path,
                    handler_name=_handler_name(path, method),
                    request_schema=request_schema,
                    responses=responses,
                    query_params=query_params,
                )
            )

    return operations


def _write_if_missing(path: Path, content: str) -> None:
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding='utf-8')


def _write_always(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding='utf-8')


def _render_struct(name: str, fields: list[Field]) -> str:
    lines = [f'struct {name} {{']
    for fld in fields:
        suffix = '' if fld.required else ' = {}'
        if fld.cpp_type == 'std::string':
            suffix = '' if fld.required else ' = ""'
        lines.append(f'    {fld.cpp_type} {fld.cpp_name}{suffix};')
    lines.append('};')
    return '\n'.join(lines)


def _render_json_serializer(schema: Schema) -> str:
    lines = [
        f'inline std::string ToJson(const {schema.name}& value) {{',
        '    userver::formats::json::ValueBuilder builder;',
    ]
    for fld in schema.fields:
        if fld.cpp_type == 'std::string':
            lines.append(f'    builder["{fld.json_name}"] = value.{fld.cpp_name};')
        elif fld.cpp_type == 'std::int64_t':
            lines.append(f'    builder["{fld.json_name}"] = value.{fld.cpp_name};')
        elif fld.cpp_type == 'bool':
            lines.append(f'    builder["{fld.json_name}"] = value.{fld.cpp_name};')
        else:
            lines.append(f'    builder["{fld.json_name}"] = value.{fld.cpp_name};')
    lines.extend(
        [
            '    return userver::formats::json::ToString(builder.ExtractValue());',
            '}',
        ]
    )
    return '\n'.join(lines)


def _render_requests_header(op: Operation, ns: str, service_name: str) -> str:
    structs: list[str] = []
    if op.query_params:
        structs.append(_render_struct('Request', op.query_params))
    if op.request_schema and op.request_schema.fields:
        structs.append(_render_struct(op.request_schema.name, op.request_schema.fields))

    if not structs:
        structs.append('struct Request {};')

    body = '\n\n'.join(structs)
    return f'''#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <userver/formats/json/value.hpp>

namespace {service_name}::views::{ns} {{

{body}

}}  // namespace {service_name}::views::{ns}
'''


def _response_type_name(status: int) -> str:
    return f'Response{status}'


def _collect_unique_schemas(responses: list[ResponseSpec]) -> list[Schema]:
    seen: set[str] = set()
    result: list[Schema] = []
    for response in responses:
        if response.schema is None or response.schema.name in seen:
            continue
        seen.add(response.schema.name)
        result.append(response.schema)
    return result


def _render_responses_header(op: Operation, ns: str, service_name: str) -> str:
    schemas = _collect_unique_schemas(op.responses)
    if not schemas:
        body = 'struct Response200 {\n    std::string body;\n};'
        response_type = 'using Response = Response200;'
        serializer_block = ''
    else:
        struct_blocks = [_render_struct(schema.name, schema.fields) for schema in schemas]
        serializers = [_render_json_serializer(schema) for schema in schemas]
        aliases = [
            f'using {_response_type_name(response.status)} = {response.schema.name};'
            for response in op.responses
            if response.schema is not None
        ]
        variant_members = [
            _response_type_name(response.status)
            for response in op.responses
            if response.schema is not None
        ]
        if len(variant_members) == 1:
            response_type = f'using Response = {variant_members[0]};'
        else:
            response_type = f'using Response = std::variant<{", ".join(variant_members)}>;'

        body = '\n\n'.join(struct_blocks)
        serializer_block = '\n\n' + '\n\n'.join(serializers)
        body = body + '\n\n' + '\n'.join(aliases) + '\n\n' + response_type

    includes = ['<cstdint>', '<string>', '<vector>']
    if len(op.responses) > 1:
        includes.append('<variant>')

    includes_block = '\n'.join(f'#include {inc}' for inc in includes)

    return f'''#pragma once

{includes_block}

#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>

namespace {service_name}::views::{ns} {{

{body}{serializer_block}

}}  // namespace {service_name}::views::{ns}
'''


def _render_handler_header(op: Operation, ns: str, service_name: str) -> str:
    return f'''#pragma once

#include <views/{op.rel_path}/requests.hpp>
#include <views/{op.rel_path}/responses.hpp>

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

namespace {service_name}::views::{ns} {{

class Handler final : public userver::server::handlers::HttpHandlerBase {{
public:
    static constexpr std::string_view kName = "{op.handler_name}";

    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override;
}};

}}  // namespace {service_name}::views::{ns}
'''


def _render_handler_visit_cases(responses: list[ResponseSpec]) -> str:
    cases: list[str] = []
    for response in responses:
        if response.schema is None:
            continue
        type_name = _response_type_name(response.status)
        status_name = HTTP_STATUS_NAMES.get(response.status)
        if status_name:
            status_line = (
                f'request_http.SetResponseStatus(userver::server::http::HttpStatus::{status_name});'
            )
        else:
            status_line = (
                f'request_http.SetResponseStatus(userver::server::http::HttpStatus{{{response.status}}});'
            )

        if response.status == 200:
            cases.append(
                f'''        if constexpr (std::is_same_v<Body, {type_name}>) {{
            return ToJson(body);
        }}'''
            )
        else:
            cases.append(
                f'''        if constexpr (std::is_same_v<Body, {type_name}>) {{
            {status_line}
            return ToJson(body);
        }}'''
            )

    cases.append('        return {};')
    return '\n'.join(cases)


def _render_handler_cpp(op: Operation, ns: str, service_name: str) -> str:
    parse_lines: list[str] = []
    for param in op.query_params:
        if param.cpp_type == 'std::string':
            parse_lines.append(
                f'    request.{param.cpp_name} = request_http.GetArg("{param.json_name}");'
            )
        elif param.cpp_type == 'std::int64_t':
            parse_lines.append(
                f'    request.{param.cpp_name} = std::stoll(request_http.GetArg("{param.json_name}"));'
            )
        elif param.cpp_type == 'bool':
            parse_lines.append(
                f'    request.{param.cpp_name} = request_http.GetArg("{param.json_name}") == "true";'
            )

    parse_block = '\n'.join(parse_lines) if parse_lines else '    (void)request_http;'

    typed_responses = [response for response in op.responses if response.schema is not None]

    if len(typed_responses) <= 1:
        response_return = '''const auto response = View::Handle(std::move(request), context);
    return ToJson(response);'''
    else:
        visit_cases = _render_handler_visit_cases(op.responses)
        response_return = f'''const auto response = View::Handle(std::move(request), context);
    return std::visit([&](const auto& body) {{
        using Body = std::decay_t<decltype(body)>;
{visit_cases}
    }}, response);'''

    extra_includes = ''
    if len(typed_responses) > 1:
        extra_includes = '''
#include <type_traits>

#include <userver/server/http/http_status.hpp>'''

    return f'''#include <views/{op.rel_path}/handler.hpp>

#include <views/{op.rel_path}/view.hpp>{extra_includes}

namespace {service_name}::views::{ns} {{

std::string Handler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request_http,
    userver::server::request::RequestContext& context) const {{
    Request request{{}};
{parse_block}

    {response_return}
}}

}}  // namespace {service_name}::views::{ns}
'''


def _render_view_header(op: Operation, ns: str, service_name: str) -> str:
    return f'''#pragma once

#include <views/{op.rel_path}/requests.hpp>
#include <views/{op.rel_path}/responses.hpp>

#include <userver/server/request/request_context.hpp>

namespace {service_name}::views::{ns} {{

class View {{
public:
    static Response Handle(
        Request&& request,
        userver::server::request::RequestContext& context);
}};

}}  // namespace {service_name}::views::{ns}
'''


def _render_view_cpp(op: Operation, ns: str, service_name: str) -> str:
    success = next((r for r in op.responses if r.status == 200 and r.schema), None)
    if success and success.schema:
        inits: list[str] = []
        for fld in success.schema.fields:
            if fld.cpp_type == 'std::string':
                inits.append(f'.{fld.cpp_name} = ""')
            else:
                inits.append(f'.{fld.cpp_name} = {{}}')
        return_stmt = f'return {_response_type_name(200)}{{{", ".join(inits)}}};'
    elif op.responses and op.responses[0].schema:
        response = op.responses[0]
        inits = [f'.{fld.cpp_name} = ""' if fld.cpp_type == 'std::string' else f'.{fld.cpp_name} = {{}}'
                 for fld in response.schema.fields]
        return_stmt = f'return {_response_type_name(response.status)}{{{", ".join(inits)}}};'
    else:
        return_stmt = 'return Response200{.body = "Not implemented\\n"};'

    return f'''#include <views/{op.rel_path}/view.hpp>

namespace {service_name}::views::{ns} {{

Response View::Handle(
    Request&& /*request*/,
    userver::server::request::RequestContext& /*context*/) {{
    {return_stmt}
}}

}}  // namespace {service_name}::views::{ns}
'''


def _render_config_fragment(operations: list[Operation]) -> str:
    lines = ['components_manager:', '    components:']
    for op in operations:
        lines.extend(
            [
                f'        {op.handler_name}:',
                f'            path: {op.path}',
                f'            method: {op.method}',
                '            task_processor: main-task-processor',
            ]
        )
    return '\n'.join(lines) + '\n'


def _render_handlers_list(operations: list[Operation], service_name: str) -> str:
    includes = '\n'.join(f'#include <views/{op.rel_path}/handler.hpp>' for op in operations)
    appends = '\n'.join(
        f'        .Append<{service_name}::views::{_cpp_namespace(op.rel_path)}::Handler>()'
        for op in operations
    )
    return f'''#pragma once

#include <userver/components/component_list.hpp>

{includes}

namespace {service_name}::openapi {{

inline auto AppendGeneratedHandlers(userver::components::ComponentList& component_list) {{
    return std::move(component_list){appends};
}}

}}  // namespace {service_name}::openapi
'''


def generate(service_dir: Path) -> list[Operation]:
    spec_dir = service_dir / 'docs' / 'api'
    gen_dir = service_dir / '.gen'
    views_dir = service_dir / 'src' / 'views'
    service_name = service_dir.name

    operations: list[Operation] = []
    for spec_path in _load_specs(spec_dir):
        operations.extend(_collect_operations(_load_openapi(spec_path)))

    if not operations:
        raise SystemExit(f'No HTTP operations found in {spec_dir}')

    for op in operations:
        ns = _cpp_namespace(op.rel_path)
        gen_include = gen_dir / 'include' / 'views' / op.rel_path
        gen_src = gen_dir / 'src' / 'views' / op.rel_path
        view_dir = views_dir / op.rel_path

        _write_always(gen_include / 'requests.hpp', _render_requests_header(op, ns, service_name))
        _write_always(gen_include / 'responses.hpp', _render_responses_header(op, ns, service_name))
        _write_always(gen_include / 'handler.hpp', _render_handler_header(op, ns, service_name))
        _write_always(gen_src / 'handler.cpp', _render_handler_cpp(op, ns, service_name))

        _write_if_missing(view_dir / 'view.hpp', _render_view_header(op, ns, service_name))
        _write_if_missing(view_dir / 'view.cpp', _render_view_cpp(op, ns, service_name))

    _write_always(
        gen_dir / 'include' / 'openapi' / 'handlers.hpp',
        _render_handlers_list(operations, service_name),
    )
    _write_always(gen_dir / 'config.openapi.yaml', _render_config_fragment(operations))
    return operations


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--service-dir',
        type=Path,
        required=True,
        help='Path to service directory (e.g. services/broker)',
    )
    args = parser.parse_args()

    service_dir = args.service_dir.resolve()
    if not service_dir.is_dir():
        raise SystemExit(f'Service directory not found: {service_dir}')

    operations = generate(service_dir)
    rel = service_dir.name
    print(f'Generated {len(operations)} handler(s) for {rel}:')
    for op in operations:
        print(f'  {op.method} {op.path} -> src/views/{op.rel_path}')


if __name__ == '__main__':
    main()
