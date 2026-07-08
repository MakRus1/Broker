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
    plain_text: bool = False



@dataclass
class Operation:
    path: str
    method: str
    operation_id: str
    rel_path: str
    request_schema: Schema | None
    responses: list[ResponseSpec]
    query_params: list[Field]


@dataclass
class HandlerGroup:
    path: str
    rel_path: str
    handler_name: str
    methods: list[str]
    request_schema: Schema | None
    responses: list[ResponseSpec]
    query_params: list[Field]
    has_deps: bool = False


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
                plain_text = False
                for content_type, body in response.get('content', {}).items():
                    schema = body.get('schema', {})
                    if content_type == 'text/plain' and schema.get('type') == 'string':
                        response_schema = Schema(
                            name='PlainTextResponse',
                            fields=[
                                Field(
                                    cpp_name='body',
                                    json_name='body',
                                    cpp_type='std::string',
                                    required=True,
                                )
                            ],
                        )
                        plain_text = True
                    elif '$ref' in schema:
                        name = schema['$ref'].rsplit('/', 1)[-1]
                        response_schema = _schema_from_components(name, schema, schemas)
                    break
                if response_schema:
                    responses.append(
                        ResponseSpec(
                            status=status,
                            schema=response_schema,
                            plain_text=plain_text,
                        )
                    )

            responses.sort(key=lambda item: item.status)

            operations.append(
                Operation(
                    path=path,
                    method=method.upper(),
                    operation_id=operation_id,
                    rel_path=rel_path,
                    request_schema=request_schema,
                    responses=responses,
                    query_params=query_params,
                )
            )

    return operations


def _group_operations(operations: list[Operation]) -> list[HandlerGroup]:
    grouped: dict[str, list[Operation]] = {}
    for op in operations:
        grouped.setdefault(op.rel_path, []).append(op)

    result: list[HandlerGroup] = []
    for rel_path in sorted(grouped):
        ops = sorted(grouped[rel_path], key=lambda item: item.method)
        path = ops[0].path
        methods = [op.method for op in ops]
        slug = rel_path.replace('/', '-')
        if len(methods) == 1:
            handler_name = f'handler-{slug}-{methods[0].lower()}'
        else:
            handler_name = f'handler-{slug}'

        result.append(
            HandlerGroup(
                path=path,
                rel_path=rel_path,
                handler_name=handler_name,
                methods=methods,
                request_schema=ops[0].request_schema,
                responses=ops[0].responses,
                query_params=ops[0].query_params,
            )
        )

    return result


def _has_deps(view_dir: Path) -> bool:
    return (view_dir / 'deps.hpp').exists()


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


def _render_requests_header(group: HandlerGroup, ns: str, service_name: str) -> str:
    structs: list[str] = []
    if group.query_params:
        structs.append(_render_struct('Request', group.query_params))
    if group.request_schema and group.request_schema.fields:
        structs.append(_render_struct(group.request_schema.name, group.request_schema.fields))

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


def _render_responses_header(group: HandlerGroup, ns: str, service_name: str) -> str:
    schemas = _collect_unique_schemas(group.responses)
    plain_only = all(response.plain_text for response in group.responses if response.schema)
    if not schemas:
        body = 'struct Response200 {\n    std::string body;\n};'
        response_type = 'using Response = Response200;'
        serializer_block = ''
    else:
        struct_blocks = [_render_struct(schema.name, schema.fields) for schema in schemas]
        serializers = [_render_json_serializer(schema) for schema in schemas]
        aliases = [
            f'using {_response_type_name(response.status)} = {response.schema.name};'
            for response in group.responses
            if response.schema is not None
        ]
        variant_members = [
            _response_type_name(response.status)
            for response in group.responses
            if response.schema is not None
        ]
        if len(variant_members) == 1:
            response_type = f'using Response = {variant_members[0]};'
        else:
            response_type = f'using Response = std::variant<{", ".join(variant_members)}>;'

        body = '\n\n'.join(struct_blocks)
        serializer_block = ''
        if not plain_only:
            serializer_block = '\n\n' + '\n\n'.join(serializers)
        body = body + '\n\n' + '\n'.join(aliases) + '\n\n' + response_type

    includes = ['<cstdint>', '<string>', '<vector>']
    if len(group.responses) > 1:
        includes.append('<variant>')

    includes_block = '\n'.join(f'#include {inc}' for inc in includes)

    return f'''#pragma once

{includes_block}

#include <userver/formats/json/value.hpp>
{'#include <userver/formats/json/value_builder.hpp>' if not plain_only else ''}
{'#include <userver/formats/json/serialize.hpp>' if not plain_only else ''}

namespace {service_name}::views::{ns} {{

{body}{serializer_block}

}}  // namespace {service_name}::views::{ns}
'''


def _response_body_expr(response: ResponseSpec, body_var: str = 'body') -> str:
    if response.plain_text:
        return f'return {body_var}.body;'
    return f'return ToJson({body_var});'


def _render_handler_header(group: HandlerGroup, ns: str, service_name: str) -> str:
    postgres_includes = ''
    postgres_member = ''
    deps_includes = ''
    constructor_decl = '    using HttpHandlerBase::HttpHandlerBase;'
    deps_member = ''

    if group.has_deps:
        deps_includes = f'''
#include <views/{group.rel_path}/deps.hpp>'''
        constructor_decl = '''
    Handler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context);'''
        deps_member = '''

private:
    Deps deps_;'''

    return f'''#pragma once

#include <views/{group.rel_path}/requests.hpp>
#include <views/{group.rel_path}/responses.hpp>

#include <userver/components/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>{deps_includes}

namespace {service_name}::views::{ns} {{

class Handler final : public userver::server::handlers::HttpHandlerBase {{
public:
    static constexpr std::string_view kName = "{group.handler_name}";
{constructor_decl}

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override;{deps_member}
}};

}}  // namespace {service_name}::views::{ns}
'''


def _render_handler_visit_cases(responses: list[ResponseSpec]) -> str:
    cases: list[str] = []
    response_by_schema: dict[str, ResponseSpec] = {}
    for response in responses:
        if response.schema is not None:
            response_by_schema[response.schema.name] = response

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

        body_expr = _response_body_expr(response, 'body')
        if response.status == 200:
            cases.append(
                f'''        if constexpr (std::is_same_v<Body, {schema_name}>) {{
            {body_expr}
        }}'''
            )
        else:
            cases.append(
                f'''        if constexpr (std::is_same_v<Body, {schema_name}>) {{
            {status_line}
            {body_expr}
        }}'''
            )

    cases.append(
        '        throw std::logic_error("Unexpected OpenAPI response variant alternative");'
    )
    return '\n'.join(cases)


def _render_handler_cpp(group: HandlerGroup, ns: str, service_name: str) -> str:
    parse_lines: list[str] = []
    for param in group.query_params:
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

    typed_responses = [response for response in group.responses if response.schema is not None]
    single_response = typed_responses[0] if len(typed_responses) == 1 else None

    view_call = 'View::Handle(std::move(request), context'
    if group.has_deps:
        view_call += ', deps_'
    view_call += ')'

    if len(typed_responses) <= 1 and single_response:
        body_expr = _response_body_expr(single_response, 'response')
        response_return = f'''const auto response = {view_call};
    {body_expr};'''
    else:
        visit_cases = _render_handler_visit_cases(group.responses)
        response_return = f'''const auto response = {view_call};
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

    deps_cpp = ''
    if group.has_deps:
        deps_cpp = '''

Handler::Handler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& component_context)
    : HttpHandlerBase(config, component_context),
      deps_(ResolveDeps(component_context)) {}
'''

    deps_include = f'\n#include <views/{group.rel_path}/deps.hpp>' if group.has_deps else ''

    return f'''#include <views/{group.rel_path}/handler.hpp>

#include <views/{group.rel_path}/view.hpp>{deps_include}{extra_includes}

namespace {service_name}::views::{ns} {{{deps_cpp}

std::string Handler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request_http,
    userver::server::request::RequestContext& context) const {{
    Request request{{}};
{parse_block}

    {response_return}
}}

}}  // namespace {service_name}::views::{ns}
'''


def _render_view_header(group: HandlerGroup, ns: str, service_name: str) -> str:
    return f'''#pragma once

#include <views/{group.rel_path}/requests.hpp>
#include <views/{group.rel_path}/responses.hpp>

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


def _render_view_cpp(group: HandlerGroup, ns: str, service_name: str) -> str:
    success = next((r for r in group.responses if r.status == 200 and r.schema), None)
    if success and success.schema:
        inits: list[str] = []
        for fld in success.schema.fields:
            if fld.cpp_type == 'std::string':
                inits.append(f'.{fld.cpp_name} = ""')
            else:
                inits.append(f'.{fld.cpp_name} = {{}}')
        return_stmt = f'return {_response_type_name(200)}{{{", ".join(inits)}}};'
    elif group.responses and group.responses[0].schema:
        response = group.responses[0]
        inits = [f'.{fld.cpp_name} = ""' if fld.cpp_type == 'std::string' else f'.{fld.cpp_name} = {{}}'
                 for fld in response.schema.fields]
        return_stmt = f'return {_response_type_name(response.status)}{{{", ".join(inits)}}};'
    else:
        return_stmt = 'return Response200{.body = "Not implemented\\n"};'

    handle_params = 'Request&& /*request*/,\n    userver::server::request::RequestContext& /*context*/'

    return f'''#include <views/{group.rel_path}/view.hpp>

namespace {service_name}::views::{ns} {{

Response View::Handle(
    {handle_params}) {{
    {return_stmt}
}}

}}  // namespace {service_name}::views::{ns}
'''


def _render_openapi_handlers_block(groups: list[HandlerGroup]) -> str:
    lines: list[str] = []
    for group in groups:
        methods = ','.join(group.methods)
        lines.extend(
            [
                f'        {group.handler_name}:',
                f'            path: {group.path}',
                f'            method: {methods}',
                '            task_processor: main-task-processor',
            ]
        )
    return '\n'.join(lines)


def _render_config_fragment(groups: list[HandlerGroup]) -> str:
    return (
        'components_manager:\n'
        '    components:\n'
        + '\n'.join(
            line
            for line in _render_openapi_handlers_block(groups).splitlines()
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


def sync_static_config_handlers(service_dir: Path, groups: list[HandlerGroup]) -> None:
    static_path = service_dir / 'configs' / 'static_config.yaml'
    if not static_path.exists():
        raise SystemExit(f'Missing static config: {static_path}')

    text = static_path.read_text(encoding='utf-8')
    if OPENAPI_HANDLERS_BEGIN not in text or OPENAPI_HANDLERS_END not in text:
        raise SystemExit(
            f'Add markers {OPENAPI_HANDLERS_BEGIN} / {OPENAPI_HANDLERS_END} '
            f'to {static_path} under components_manager.components'
        )

    block = _render_openapi_handlers_block(groups)
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


def _render_handlers_list(groups: list[HandlerGroup], service_name: str) -> str:
    includes = '\n'.join(f'#include <views/{group.rel_path}/handler.hpp>' for group in groups)
    appends = '\n'.join(
        f'        .Append<{service_name}::views::{_cpp_namespace(group.rel_path)}::Handler>()'
        for group in groups
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


def _cleanup_stale_generated_handlers(gen_dir: Path, active_rel_paths: set[str]) -> None:
    import shutil

    for base in (gen_dir / 'include' / 'views', gen_dir / 'src' / 'views'):
        if not base.exists():
            continue
        for handler_marker in list(base.glob('**/handler.hpp')) + list(base.glob('**/handler.cpp')):
            rel_path = handler_marker.parent.relative_to(base).as_posix()
            if rel_path not in active_rel_paths:
                shutil.rmtree(handler_marker.parent)


def generate(service_dir: Path) -> list[HandlerGroup]:
    spec_dir = service_dir / 'docs' / 'api'
    gen_dir = service_dir / '.gen'
    views_dir = service_dir / 'src' / 'views'
    service_name = service_dir.name

    operations: list[Operation] = []
    for spec_path in _load_specs(spec_dir):
        operations.extend(_collect_operations(_load_openapi(spec_path)))

    if not operations:
        raise SystemExit(f'No HTTP operations found in {spec_dir}')

    groups = _group_operations(operations)

    for group in groups:
        ns = _cpp_namespace(group.rel_path)
        gen_include = gen_dir / 'include' / 'views' / group.rel_path
        gen_src = gen_dir / 'src' / 'views' / group.rel_path
        view_dir = views_dir / group.rel_path

        group.has_deps = _has_deps(view_dir)

        _write_always(gen_include / 'requests.hpp', _render_requests_header(group, ns, service_name))
        _write_always(gen_include / 'responses.hpp', _render_responses_header(group, ns, service_name))
        _write_always(gen_include / 'handler.hpp', _render_handler_header(group, ns, service_name))
        _write_always(gen_src / 'handler.cpp', _render_handler_cpp(group, ns, service_name))

        _write_if_missing(view_dir / 'view.hpp', _render_view_header(group, ns, service_name))
        _write_if_missing(view_dir / 'view.cpp', _render_view_cpp(group, ns, service_name))

    _cleanup_stale_generated_handlers(gen_dir, {group.rel_path for group in groups})

    _write_always(
        gen_dir / 'include' / 'openapi' / 'handlers.hpp',
        _render_handlers_list(groups, service_name),
    )
    _write_always(gen_dir / 'config.openapi.yaml', _render_config_fragment(groups))
    return groups


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


def compute_codegen_digest(service_dir: Path, groups: list[HandlerGroup]) -> str:
    spec_dir = service_dir / 'docs' / 'api'
    hasher = hashlib.sha256()

    for path in _spec_files(spec_dir):
        _hash_file(hasher, path, path.relative_to(spec_dir).as_posix())
    for path in _generator_inputs():
        _hash_file(hasher, path, path.relative_to(GENERATOR_DIR).as_posix())

    hasher.update(_render_openapi_handlers_block(groups).encode())

    for group in groups:
        hasher.update(
            f'{group.handler_name}\0{",".join(group.methods)}\0{group.path}\0{group.rel_path}\n'.encode()
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


def verify_static_config(service_dir: Path, groups: list[HandlerGroup]) -> list[str]:
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

    expected = _render_openapi_handlers_block(groups).strip()
    if section.strip() != expected:
        return [
            f'OpenAPI handlers block in {static_config_path} is out of date (run make gen)'
        ]

    return []


def verify_view_stubs(service_dir: Path, groups: list[HandlerGroup]) -> list[str]:
    views_dir = service_dir / 'src' / 'views'
    errors: list[str] = []
    for group in groups:
        view_dir = views_dir / group.rel_path
        for name in ('view.hpp', 'view.cpp'):
            path = view_dir / name
            if not path.exists():
                errors.append(f'Missing view stub: {path} (run make gen)')
    return errors


def verify_codegen(service_dir: Path, groups: list[HandlerGroup]) -> list[str]:
    errors: list[str] = []
    expected = compute_codegen_digest(service_dir, groups)
    committed = read_codegen_lock(service_dir)
    if committed is None:
        errors.append(f'Missing {LOCK_FILE_NAME} in {service_dir / "docs" / "api"} (run make gen)')
    elif committed != expected:
        errors.append(
            f'{LOCK_FILE_NAME} is out of date for {service_dir.name} '
            f'(run make gen and commit {LOCK_FILE_NAME})'
        )
    errors.extend(verify_static_config(service_dir, groups))
    errors.extend(verify_view_stubs(service_dir, groups))
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

    groups = generate(service_dir)
    rel = service_dir.name

    if args.check:
        errors = verify_codegen(service_dir, groups)
        if errors:
            print(f'OpenAPI codegen check failed for {rel}:', file=sys.stderr)
            for error in errors:
                print(f'  - {error}', file=sys.stderr)
            raise SystemExit(1)
        print(f'OpenAPI codegen check passed for {rel}')
        return

    sync_static_config_handlers(service_dir, groups)

    digest = compute_codegen_digest(service_dir, groups)
    write_codegen_lock(service_dir, digest)

    print(f'Generated {len(groups)} handler(s) for {rel}:')
    for group in groups:
        methods = ','.join(group.methods)
        print(f'  {methods} {group.path} -> src/views/{group.rel_path}')
    print(f'Updated {LOCK_FILE_NAME} and configs/static_config.yaml')


if __name__ == '__main__':
    main()
