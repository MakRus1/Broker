"""Генерация C++ без записи файлов и неявных решений о контракте."""
import json
from pathlib import Path
import subprocess
import sys
from cpp_types import declarations, enums, literal, quoted, type_cpp, json_value
from runtime_validation import CODE as VALIDATION_RUNTIME
from model import enum_types, collect_models


def format_cpp(text):
    formatter = Path(sys.executable).parent / 'clang-format'
    try:
        return subprocess.run([str(formatter), '--style=file:' + str(Path(__file__).resolve().parents[2] / '.clang-format')],
                              input=text, text=True, capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError('Не удалось запустить закрепленный clang-format; установите requirements.txt') from error


def quote(value):
    return quoted(value)


def fields(items, scope=''):
    return '\n'.join(f'    {"std::optional<" + type_cpp(f.type, scope) + ">" if f.optional else type_cpp(f.type, scope)} {f.name}' + '{' +
        (literal(f.type, f.type.schema['default'], scope) if 'default' in f.type.schema else '') + '};' for f in items)


def presence(names):
    count = len(names)
    body = '\n'.join(f'if (name == {quote(name)}) return broker_codegen::InputPresence<{count}>::WasProvided({index});'
                     for index, name in enumerate(names))
    return 'bool WasProvided(std::string_view name) const {\n' + body + '\nreturn false;\n}'


def struct(name, items, input_metadata=False, scope=''):
    base = f' : broker_codegen::InputPresence<{len(items)}>' if input_metadata else ''
    metadata = presence([f.name for f in items]) if input_metadata else ''
    return declarations(name, items) + f'struct {name}{base} {{\n{fields(items, scope)}\n{metadata}\n}};'


def preamble(service, op, include_types=True):
    return (f'#pragma once\n#include <views/{op.rel}/types.hpp>\n' if include_types else '#pragma once\n') + f'''#include <cstdint>
#include <limits>
#include <openapi/runtime.hpp>
#include <openapi/models.hpp>
#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <platform/calendar_date.hpp>
#include <platform/timestamp.hpp>
#include <userver/utils/datetime/date.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/formats/parse/common.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/serialize/common_containers.hpp>
namespace {service}::views::{op.ns} {{
'''


def requests(service, op):
    text = preamble(service, op)
    scope = f'::{service}::views::{op.ns}'
    if op.body:
        if op.body.model_name:
            text += declarations('RequestBody', op.body.fields)
            text += f'using RequestBody = ::{service}::models::{op.body.model_name};\n'
        else:
            text += struct('RequestBody', op.body.fields, input_metadata=True, scope=scope) + '\n'
    members = fields(op.params, scope)
    if op.body:
        typ = 'RequestBody' if op.body.required else 'std::optional<RequestBody>'
        members += f'\n    {typ} body{{}};'
    names = [f.name for f in op.params] + (['body'] if op.body else [])
    return text + declarations('Request', op.params) + f'struct Request : broker_codegen::InputPresence<{len(names)}> {{\n{members}\n{presence(names)}\n}};\n\n}}  // namespace {service}::views::{op.ns}\n'


def responses(service, op):
    text = preamble(service, op)
    for r in op.responses:
        name = f'Response{r.status}'
        text += struct(name, r.body.fields if r.body else (), scope=f'::{service}::views::{op.ns}') if r.media != 'text/plain' else f'struct {name} {{ std::string body; }};'
        text += '\n'
        if r.media == 'application/json':
            text += f'inline std::string ToJson(const {name}& value) {{\n    userver::formats::json::ValueBuilder out(userver::formats::json::Type::kObject);\n'
            for f in r.body.fields:
                if not f.optional:
                    text += f'    Validate_{name}_{f.name}(value.{f.name});\n    out[{quote(f.name)}] = {json_value(f.type, "value." + f.name)};\n'
                else:
                    text += f'    if (value.{f.name}.has_value()) {{ Validate_{name}_{f.name}(value.{f.name}.value()); out[{quote(f.name)}] = {json_value(f.type, "value." + f.name + ".value()")}; }}\n'
            text += '    return userver::formats::json::ToString(out.ExtractValue());\n}\n'
    types = ', '.join(f'Response{r.status}' for r in op.responses)
    text += f'using Response = std::variant<{types}>;\n}}\n'
    return text


def view(service, op, deps):
    params = 'Request&& request, userver::server::request::RequestContext& context' + (', Deps deps' if deps else '')
    includes = '#include <deps.hpp>\n' if deps else ''
    header = f'''#pragma once
#include <views/{op.rel}/requests.hpp>
#include <views/{op.rel}/responses.hpp>
#include <userver/server/request/request_context.hpp>
{includes}
namespace {service}::views::{op.ns} {{
class View {{
public:
    static Response Handle({params});
}};
}}
'''
    params = 'Request&&, userver::server::request::RequestContext&' + (', Deps' if deps else '')
    source = f'''#include <views/{op.rel}/view.hpp>
#include <userver/server/handlers/exceptions.hpp>
namespace {service}::views::{op.ns} {{
Response View::Handle({params}) {{
    throw userver::server::handlers::InternalServerError(userver::server::handlers::ExternalBody{{"Not implemented"}});
}}
}}
'''
    return format_cpp(header), format_cpp(source)


COMMON = r'''#pragma once
#include <cmath>
#include <chrono>
#include <array>
#include <optional>
#include <string_view>
#include <userver/utils/regex.hpp>
#include <set>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <platform/calendar_date.hpp>
#include <platform/timestamp.hpp>
#include <userver/utils/datetime/date.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/formats/parse/common.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
namespace broker_codegen {
template <std::size_t N> class InputPresence {
public:
    void MarkProvided(std::size_t index) { provided_.at(index) = true; }
    bool WasProvided(std::size_t index) const { return provided_.at(index); }
private:
    std::array<bool, N> provided_{};
};
template <typename T> T ParseScalar(const std::string& text) {
    if constexpr (std::is_enum_v<T>) return ParseEnumScalar(text, T{});
    else if constexpr (std::is_same_v<T, std::string>) return text;
    else if constexpr (std::is_same_v<T, userver::utils::datetime::Date>) { return platform::ParseCalendarDate(text); }
    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) return platform::ParseTimestamp(text);
    else if constexpr (std::is_same_v<T, bool>) {
        if (text == "true") return true;
        if (text == "false") return false;
        throw std::invalid_argument("boolean");
    } else {
        if (text.empty() || text.front() == ' ' || text.front() == '+' || text.find_first_of("\t\r\n ") != std::string::npos)
            throw std::invalid_argument("number");
        std::size_t used = 0;
        if constexpr (std::is_integral_v<T>) {
            const auto value = std::stoll(text, &used);
            if (used != text.size() || value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
                throw std::invalid_argument("integer");
            return static_cast<T>(value);
        } else {
            const auto value = std::stod(text, &used);
            if (used != text.size() || !std::isfinite(value)) throw std::invalid_argument("number");
            const auto result = static_cast<T>(value);
            if (!std::isfinite(result)) throw std::invalid_argument("number");
            return result;
        }
    }
}
template <typename T> struct IsVector : std::false_type {};
template <typename T> struct IsVector<std::vector<T>> : std::true_type {};
template <typename T> struct IsOptional : std::false_type {};
template <typename T> struct IsOptional<std::optional<T>> : std::true_type {};
template <typename T> userver::formats::json::Value CalendarDateJson(const std::optional<T>&);
template <typename T> userver::formats::json::Value CalendarDateJson(const std::vector<T>&);
inline userver::formats::json::Value CalendarDateJson(userver::utils::datetime::Date date) {
    return userver::formats::json::ValueBuilder(platform::FormatCalendarDate(date)).ExtractValue();
}
template <typename T> userver::formats::json::Value CalendarDateJson(const std::optional<T>& value) {
    if (value.has_value()) return CalendarDateJson(value.value());
    return userver::formats::json::ValueBuilder{}.ExtractValue();
}
template <typename T> userver::formats::json::Value CalendarDateJson(const std::vector<T>& values) {
    userver::formats::json::ValueBuilder result(userver::formats::json::Type::kArray);
    for (const auto& value : values) result.PushBack(CalendarDateJson(value));
    return result.ExtractValue();
}
template <typename T> T ParseJson(const userver::formats::json::Value& value) {
    if constexpr (IsOptional<T>::value) {
        if (value.IsNull()) return std::nullopt;
        return T{ParseJson<typename T::value_type>(value)};
    } else if constexpr (std::is_enum_v<T>) return ParseEnumJson(value, T{});
    else if constexpr (std::is_same_v<T, userver::utils::datetime::Date>) return ParseScalar<T>(value.As<std::string>());
    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) return ParseScalar<T>(value.As<std::string>());
    else if constexpr (std::is_same_v<T, userver::formats::json::Value>) {
        if (!value.IsObject()) throw std::invalid_argument("object");
        return value;
    }
    else if constexpr (IsVector<T>::value) {
        if (!value.IsArray()) throw std::invalid_argument("array");
        T result;
        for (const auto& item : value) result.push_back(ParseJson<typename T::value_type>(item));
        return result;
    } else if constexpr (std::is_class_v<T> && !std::is_same_v<T, std::string>) {
        return ParseModelJson(value, std::type_identity<T>{});
    } else {
        if constexpr (std::is_same_v<T, std::string>) { if (!value.IsString()) throw std::invalid_argument("string"); }
        else if constexpr (std::is_same_v<T, bool>) { if (!value.IsBool()) throw std::invalid_argument("boolean"); }
        else if constexpr (std::is_integral_v<T>) { if (!value.IsInt64()) throw std::invalid_argument("integer"); }
        else { if (!value.IsDouble() && !value.IsInt64() && !value.IsUInt64()) throw std::invalid_argument("number"); }
        auto result = value.As<T>();
        if constexpr (std::is_floating_point_v<T>) { if (!std::isfinite(result)) throw std::invalid_argument("number"); }
        return result;
    }
}
}
'''


COMMON += VALIDATION_RUNTIME


def handler(service, op, deps):
    dep_include = '#include <deps.hpp>\n' if deps else ''
    ctor = 'using HttpHandlerBase::HttpHandlerBase;'
    private = ''
    ctor_source = ''
    if deps:
        ctor = 'Handler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);'
        private = 'private:\n    Deps deps_;'
        ctor_source = '''Handler::Handler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), deps_(ResolveDeps(context)) {}'''
    header = f'''#pragma once
#include <userver/server/handlers/http_handler_base.hpp>
{dep_include}
namespace {service}::views::{op.ns} {{
class Handler final : public userver::server::handlers::HttpHandlerBase {{
public:
    static constexpr std::string_view kName = "{op.handler}";
    {ctor}
    std::string HandleRequestThrow(const userver::server::http::HttpRequest&, userver::server::request::RequestContext&) const override;
{private}
}};
}}
'''
    parse = []
    for index, f in enumerate(op.params):
        if f.location == 'path':
            parse.append(f'request.{f.name} = broker_codegen::ParseScalar<{f.type.cpp}>(http.GetPathArg({quote(f.name)}));')
            parse.append(f'Validate_Request_{f.name}(request.{f.name}); request.MarkProvided({index});')
        else:
            parse.append(f'if (http.GetArgVector({quote(f.name)}).size() > 1) throw std::invalid_argument("duplicate parameter");')
            if f.required:
                parse.append(f'if (!http.HasArg({quote(f.name)})) throw std::invalid_argument("missing parameter");')
            parse.append(f'if (http.HasArg({quote(f.name)})) {{ auto value = broker_codegen::ParseScalar<{f.type.cpp}>(http.GetArg({quote(f.name)})); Validate_Request_{f.name}(value); request.{f.name} = std::move(value); request.MarkProvided({index}); }}')
    if op.body:
        if op.body.required:
            parse.append('if (http.RequestBody().empty()) throw std::invalid_argument("missing body");')
        parse += ['if (!http.RequestBody().empty()) {',
                  *(['const auto content_type = http.GetHeader("Content-Type");',
                     'if (content_type != "application/json" && content_type.rfind("application/json;", 0) != 0) throw std::invalid_argument("content type");'] if op.body.validate_content_type else []),
                  'const auto json = userver::formats::json::FromString(http.RequestBody());',
                  'if (!json.IsObject()) throw std::invalid_argument("object body");', 'RequestBody body;']
        if op.body.strict:
            allowed = ' && '.join(f'it.GetName() != {quote(f.name)}' for f in op.body.fields) or 'true'
            parse += [f'for (auto it = json.begin(); it != json.end(); ++it) if ({allowed}) throw std::invalid_argument("unknown field");']
        for index, f in enumerate(op.body.fields):
            if f.required:
                parse.append(f'if (!json.HasMember({quote(f.name)})) throw std::invalid_argument("missing field");')
            assignment = f'body.{f.name}.emplace(std::move(value))' if f.optional else f'body.{f.name} = std::move(value)'
            parse.append(f'if (json.HasMember({quote(f.name)})) {{ auto value = broker_codegen::ParseJson<{f.type.cpp}>(json[{quote(f.name)}]); Validate_RequestBody_{f.name}(value); {assignment}; body.MarkProvided({index}); }}')
        parse += [f'request.body = std::move(body); request.MarkProvided({len(op.params)});', '}']
    parse_text = '\n        '.join(parse)
    cases = []
    for r in op.responses:
        media = r.media or 'text/plain'
        ret = 'ToJson(body)' if r.media == 'application/json' else ('body.body' if r.media == 'text/plain' else 'std::string{}')
        cases.append(f'''if constexpr (std::is_same_v<T, Response{r.status}>) {{
            http.SetResponseStatus(static_cast<userver::server::http::HttpStatus>({r.status}));
            http.GetHttpResponse().SetHeader(std::string_view{{"Content-Type"}}, "{media}");
            return {ret};
        }}''')
    parse_error = 'throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{"Invalid request"});'
    bad_request = next((response for response in op.responses if response.status == 400), None)
    if bad_request and bad_request.media == 'application/json' and bad_request.body and {field.name for field in bad_request.body.fields} == {'error', 'message'} and all(field.required and field.type.schema == {'type': 'string'} for field in bad_request.body.fields):
        parse_error = 'http.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest); http.GetHttpResponse().SetHeader(std::string_view{"Content-Type"}, "application/json"); return ToJson(Response400{.error = "invalid_request", .message = "Invalid request"});'
    error_catches = []
    for response in op.responses:
        if (response.status >= 400 and response.media == 'application/json' and response.body
                and {field.name for field in response.body.fields} == {'error', 'message'}
                and all(field.required and field.type.kind == 'string' and not field.optional for field in response.body.fields)):
            error_catches.append(f'catch (const ::platform::http::HandlerError<{response.status}>& error) {{ return Response{response.status}{{.error = error.Code, .message = error.what()}}; }}')
    invoke = "View::Handle(std::move(request), context" + (", deps_" if deps else "") + ")"
    response_call = ('[&]() -> Response { try { return ' + invoke + '; } '
                     + ' '.join(error_catches) + ' }()') if error_catches else invoke
    auth_include = '#include <platform/auth/identity.hpp>\n' if op.authenticated else ''
    auth_check = '(void)::platform::auth::GetIdentity(context);' if op.authenticated else ''
    source = f'''{auth_include}#include <views/{op.rel}/handler.hpp>
#include <views/{op.rel}/view.hpp>
#include <openapi/runtime.hpp>
#include <platform/handler_error.hpp>
#include <utility>
#include <new>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/http/http_response.hpp>
namespace {service}::views::{op.ns} {{
{ctor_source}
std::string Handler::HandleRequestThrow(const userver::server::http::HttpRequest& http, userver::server::request::RequestContext& context) const {{
    {auth_check}
    Request request{{}};
    try {{
        {parse_text}
    }} catch (const std::bad_alloc&) {{
        throw;
    }} catch (const std::exception&) {{
        {parse_error}
    }}
    const auto response = {response_call};
    return std::visit([&](const auto& body) -> std::string {{
        using T = std::decay_t<decltype(body)>;
        {' '.join(cases)}
    }}, response);
}}
}}
'''
    return format_cpp(header), format_cpp(source)


def outputs(service, operations, dependencies):
    from models_render import render_models, bind_operation
    result = {'include/openapi/runtime.hpp': COMMON,
              'include/openapi/models.hpp': render_models(collect_models(operations), f'{service}::models')}
    includes = []
    appends = []
    for op in operations:
        op = bind_operation(op, f'{service}::models')
        base = f'include/views/{op.rel}'
        result[f'{base}/types.hpp'] = preamble(service, op, include_types=False) + ''.join(enums(typ) for typ in enum_types(op)) + f'\n}}  // namespace {service}::views::{op.ns}\n'
        result[f'{base}/requests.hpp'] = requests(service, op)
        result[f'{base}/responses.hpp'] = responses(service, op)
        hpp, cpp = handler(service, op, op.rel in dependencies)
        result[f'{base}/handler.hpp'] = hpp
        result[f'src/views/{op.rel}/handler.cpp'] = cpp
        includes.append(f'#include <views/{op.rel}/handler.hpp>')
        appends.append(f'.Append<{service}::views::{op.ns}::Handler>()')
    result['include/openapi/handlers.hpp'] = '#pragma once\n#include <utility>\n#include <new>\n#include <userver/components/component_list.hpp>\n' + '\n'.join(includes) + f'\nnamespace {service}::openapi {{\ninline auto AppendGeneratedHandlers(userver::components::ComponentList& list) {{\n return std::move(list)' + ''.join(appends) + ';\n}\n}\n'
    return {name: format_cpp(text) for name, text in result.items()}


def config_block(operations, logging=None):
    from http_logging import body_limits
    handlers = []
    for operation in operations:
        request_limit, response_limit = body_limits(operation.path, logging)
        lines = [
            f'        {operation.handler}:',
            f'            path: {operation.path}',
            f'            method: {operation.method}',
        ]
        if operation.path.startswith('/admin/'):
            lines.append('            monitor-handler: true')
        lines.extend([
            '            task_processor: main-task-processor',
            f'            request_body_size_log_limit: {request_limit}',
            f'            response_data_size_log_limit: {response_limit}',
        ])
        handlers.append('\n'.join(lines))
    return '\n'.join(handlers)
