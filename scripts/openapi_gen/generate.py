#!/usr/bin/env python3
"""Generate userver handlers and DTOs from OpenAPI specs in docs/api/."""

from __future__ import annotations

import argparse
import hashlib
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
LOCK_FILE_NAME = 'codegen.lock'
GENERATOR_DIR = Path(__file__).resolve().parent
OPENAPI_HANDLERS_BEGIN = '# OPENAPI_HANDLERS_BEGIN'
OPENAPI_HANDLERS_END = '# OPENAPI_HANDLERS_END'


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
        schema_name = response.schema.name
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
                f'''        if constexpr (std::is_same_v<Body, {schema_name}>) {{
            return ToJson(body);
        }}'''
            )
        else:
            cases.append(
                f'''        if constexpr (std::is_same_v<Body, {schema_name}>) {{
            {status_line}
            return ToJson(body);
        }}'''
            )

    cases.append(
        '        throw std::logic_error("Unexpected OpenAPI response variant alternative");'
    )
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
    return std::visit([&](const auto& body) -> std::string {{
        using Body = std::decay_t<decltype(body)>;
{visit_cases}
    }}, response);'''

    extra_includes = ''
    if len(typed_responses) > 1:
        extra_includes = '''
#include <stdexcept>
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


def _render_openapi_handlers_block(operations: list[Operation]) -> str:
    lines: list[str] = []
    for op in operations:
        lines.extend(
            [
                f'        {op.handler_name}:',
                f'            path: {op.path}',
                f'            method: {op.method}',
                '            task_processor: main-task-processor',
            ]
        )
    return '\n'.join(lines)


def _render_config_fragment(operations: list[Operation]) -> str:
    return (
        'components_manager:\n'
        '    components:\n'
        + '\n'.join(
            line
            for line in _render_openapi_handlers_block(operations).splitlines()
            if line.strip()
        )
        + '\n'
    )


def _extract_openapi_handlers_section(static_config_text: str) -> str | None:
    begin = static_config_text.find(OPENAPI_HANDLERS_BEGIN)
    end = static_config_text.find(OPENAPI_HANDLERS_END)
    if begin == -1 or end == -1 or end < begin:
        return None
    return static_config_text[begin + len(OPENAPI_HANDLERS_BEGIN):end].strip('\n')


def sync_static_config_handlers(service_dir: Path, operations: list[Operation]) -> None:
    static_path = service_dir / 'configs' / 'static_config.yaml'
    if not static_path.exists():
        raise SystemExit(f'Missing static config: {static_path}')

    text = static_path.read_text(encoding='utf-8')
    if OPENAPI_HANDLERS_BEGIN not in text or OPENAPI_HANDLERS_END not in text:
        raise SystemExit(
            f'Add markers {OPENAPI_HANDLERS_BEGIN} / {OPENAPI_HANDLERS_END} '
            f'to {static_path} under components_manager.components'
        )

    block = _render_openapi_handlers_block(operations)
    replacement_block = block
    if block:
        replacement_block = f'\n{block}\n        '

    pattern = re.compile(
        rf'({re.escape(OPENAPI_HANDLERS_BEGIN)})'
        rf'.*?'
        rf'(\s*{re.escape(OPENAPI_HANDLERS_END)})',
        re.DOTALL,
    )
    new_text, count = pattern.subn(
        rf'\1{replacement_block}\2',
        text,
        count=1,
    )
    if count != 1:
        raise SystemExit(f'Failed to update OpenAPI handlers in {static_path}')

    static_path.write_text(new_text, encoding='utf-8')


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


def _generator_inputs() -> list[Path]:
    return sorted(GENERATOR_DIR.glob('*.py'))


def _spec_files(spec_dir: Path) -> list[Path]:
    return sorted(
        path
        for path in spec_dir.glob('**/*')
        if path.is_file() and path.suffix in {'.yaml', '.yml', '.json'} and path.name != LOCK_FILE_NAME
    )


def _hash_file(hasher: hashlib._Hash, path: Path, label: str | None = None) -> None:
    hasher.update((label or str(path)).encode())
    hasher.update(path.read_bytes())


def compute_codegen_digest(service_dir: Path, operations: list[Operation]) -> str:
    hasher = hashlib.sha256()

    for path in _spec_files(service_dir / 'docs' / 'api'):
        _hash_file(hasher, path)
    for path in _generator_inputs():
        _hash_file(hasher, path)

    hasher.update(_render_openapi_handlers_block(operations).encode())

    for op in operations:
        hasher.update(
            f'{op.handler_name}\0{op.method}\0{op.path}\0{op.rel_path}\n'.encode()
        )

    return hasher.hexdigest()


def _lock_path(service_dir: Path) -> Path:
    return service_dir / 'docs' / 'api' / LOCK_FILE_NAME


def write_codegen_lock(service_dir: Path, digest: str) -> None:
    lock_path = _lock_path(service_dir)
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    lock_path.write_text(f'sha256={digest}\n', encoding='utf-8')


def read_codegen_lock(service_dir: Path) -> str | None:
    lock_path = _lock_path(service_dir)
    if not lock_path.exists():
        return None
    for line in lock_path.read_text(encoding='utf-8').splitlines():
        if line.startswith('sha256='):
            return line.removeprefix('sha256=').strip()
    return None


def verify_static_config(service_dir: Path, operations: list[Operation]) -> list[str]:
    static_config_path = service_dir / 'configs' / 'static_config.yaml'
    if not static_config_path.exists():
        return [f'Missing static config: {static_config_path}']

    text = static_config_path.read_text(encoding='utf-8')
    section = _extract_openapi_handlers_section(text)
    if section is None:
        return [
            f'Markers {OPENAPI_HANDLERS_BEGIN} / {OPENAPI_HANDLERS_END} '
            f'missing in {static_config_path}'
        ]

    expected = _render_openapi_handlers_block(operations).strip()
    if section.strip() != expected:
        return [
            f'OpenAPI handlers block in {static_config_path} is out of date (run make gen)'
        ]

    return []


def verify_view_stubs(service_dir: Path, operations: list[Operation]) -> list[str]:
    views_dir = service_dir / 'src' / 'views'
    errors: list[str] = []
    for op in operations:
        view_dir = views_dir / op.rel_path
        for name in ('view.hpp', 'view.cpp'):
            path = view_dir / name
            if not path.exists():
                errors.append(f'Missing view stub: {path} (run make gen)')
    return errors


def verify_codegen(service_dir: Path, operations: list[Operation]) -> list[str]:
    errors: list[str] = []
    expected = compute_codegen_digest(service_dir, operations)
    committed = read_codegen_lock(service_dir)
    if committed is None:
        errors.append(f'Missing {LOCK_FILE_NAME} in {service_dir / "docs" / "api"} (run make gen)')
    elif committed != expected:
        errors.append(
            f'{LOCK_FILE_NAME} is out of date for {service_dir.name} '
            f'(run make gen and commit {LOCK_FILE_NAME})'
        )
    errors.extend(verify_static_config(service_dir, operations))
    errors.extend(verify_view_stubs(service_dir, operations))
    return errors


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--service-dir',
        type=Path,
        required=True,
        help='Path to service directory (e.g. services/broker)',
    )
    parser.add_argument(
        '--check',
        action='store_true',
        help='Verify codegen.lock, static_config.yaml and view stubs (implies generation)',
    )
    args = parser.parse_args()

    service_dir = args.service_dir.resolve()
    if not service_dir.is_dir():
        raise SystemExit(f'Service directory not found: {service_dir}')

    operations = generate(service_dir)
    rel = service_dir.name

    if args.check:
        errors = verify_codegen(service_dir, operations)
        if errors:
            print(f'OpenAPI codegen check failed for {rel}:', file=sys.stderr)
            for error in errors:
                print(f'  - {error}', file=sys.stderr)
            raise SystemExit(1)
        print(f'OpenAPI codegen check passed for {rel}')
        return

    sync_static_config_handlers(service_dir, operations)

    digest = compute_codegen_digest(service_dir, operations)
    write_codegen_lock(service_dir, digest)

    print(f'Generated {len(operations)} handler(s) for {rel}:')
    for op in operations:
        print(f'  {op.method} {op.path} -> src/views/{op.rel_path}')
    print(f'Updated {LOCK_FILE_NAME} and configs/static_config.yaml')


if __name__ == '__main__':
    main()
