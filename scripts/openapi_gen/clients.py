"""HTTP-клиенты: независимые DTO, проверка контракта и внедряемый транспорт."""
from pathlib import Path
import re
import yaml
from model import identifier, enum_types, collect_models
from configs import http_name
from cpp_types import enums, declarations, quoted, type_cpp
from render import fields, format_cpp

ROOT = Path(__file__).resolve().parents[2]


def client_specs(service, loader):
    manifest = service / 'service.yaml'
    if not manifest.exists():
        return []
    spec = yaml.load(manifest.read_text(), Loader=loader)
    clients = spec.get('clients', {}) if isinstance(spec, dict) else None
    if not isinstance(clients, dict):
        raise ValueError('service.yaml: clients должен быть объектом')
    result = []
    for alias, entry in sorted(clients.items()):
        identifier(alias)
        if not re.fullmatch(r'[a-z][a-z0-9_]*', alias):
            raise ValueError(f'Имя клиента должно состоять из строчных букв, цифр и _: {alias}')
        if alias in {'pg_cluster', 'configs', 'readiness', 'ResolveDeps'}:
            raise ValueError(f'Имя клиента зарезервировано: {alias}')
        if not isinstance(entry, dict) or set(entry) != {'service', 'transport'} or entry['transport'] != 'http':
            raise ValueError(f'clients.{alias}: ожидаются service и transport: http')
        name = entry['service']
        if not isinstance(name, str) or not re.fullmatch(r'[a-z][a-z0-9_]*', name):
            raise ValueError(f'clients.{alias}: некорректное имя сервиса')
        provider = ROOT / 'services' / name
        if not provider.is_dir() or not provider.resolve().is_relative_to((ROOT / 'services').resolve()):
            raise ValueError(f'clients.{alias}: неизвестный сервис {name}')
        result.append((alias, provider))
    return result


def dto(name, items, scope):
    return declarations(name, items) + f'struct {name} {{\n{fields(items, scope)}\n}};\n'


def json_write(name, items):
    lines = ['userver::formats::json::ValueBuilder json(userver::formats::json::Type::kObject);']
    for f in items:
        value = f'*value.{f.name}' if f.optional else f'value.{f.name}'
        line = f'Validate_{name}_{f.name}({value}); json[{quoted(f.name)}] = {value};'
        lines.append(f'if (value.{f.name}) {{ {line} }}' if f.optional else line)
    return '\n'.join(lines) + '\nreturn userver::formats::json::ToString(json.ExtractValue());'


def json_fields(name, body):
    lines = ['if (!json.IsObject()) throw std::invalid_argument("object expected");', f'{name} value;']
    if body.strict:
        allowed = ' && '.join(f'it.GetName() != {quoted(f.name)}' for f in body.fields) or 'true'
        lines.append(f'for (auto it = json.begin(); it != json.end(); ++it) if ({allowed}) throw std::invalid_argument("unknown field");')
    for f in body.fields:
        key = quoted(f.name)
        if f.required:
            lines.append(f'if (!json.HasMember({key})) throw std::invalid_argument("missing field");')
        assignment = f'value.{f.name}.emplace(std::move(item))' if f.optional else f'value.{f.name} = std::move(item)'
        lines.append(f'if (json.HasMember({key})) {{ auto item = broker_codegen::ParseJson<{f.type.cpp}>(json[{key}]); Validate_{name}_{f.name}(item); {assignment}; }}')
    return lines


def json_read(response, finish):
    lines = ['platform::http::RequireContentType(raw, "application/json");',
             'const auto json = userver::formats::json::FromString(raw.body);']
    lines += json_fields(f'Response{response.status}', response.body)
    return '\n'.join(lines) + '\n' + finish


def operation_code(scope, op):
    if op.cookie_name:
        raise ValueError(f'{op.path}: cookie-authenticated browser APIs are not supported by C++ clients')
    text = f'namespace {op.name} {{\n'
    types = enum_types(op)
    error_names = {f'Error{r.status}' for r in op.responses if r.status >= 300}
    if any(t.enum_name in error_names for t in types):
        raise ValueError(f'{op.name}: enum name collides with generated API error')
    text += ''.join(enums(t) for t in types)
    if op.body:
        text += dto('RequestBody', op.body.fields, scope)
        text += 'inline std::string ToJson(const RequestBody& value) {\n' + json_write('RequestBody', op.body.fields) + '\n}\n'
        text += 'inline RequestBody ParseBody(const userver::formats::json::Value& json) {\n' + '\n'.join(json_fields('RequestBody', op.body)) + '\nreturn value;\n}\n'
    text += declarations('Request', op.params) + 'struct Request {\n' + fields(op.params, scope)
    if op.body:
        text += '\n' + ('RequestBody' if op.body.required else 'std::optional<RequestBody>') + ' body{};'
    text += '\n};\n'
    for response in op.responses:
        name = f'Response{response.status}'
        text += (f'struct {name} {{ std::string body; }};\n' if response.media == 'text/plain' else dto(name, response.body.fields if response.body else (), scope))
        if response.media == 'application/json':
            text += f'inline std::string ToJson(const {name}& value) {{\n' + json_write(name, response.body.fields) + '\n}\n'
        if response.status >= 300:
            text += f'''class Error{response.status} final : public platform::http::ApiError {{
public:
explicit Error{response.status}(Response{response.status} value)
: ApiError({response.status}{", ToJson(value)" if response.media == "application/json" else ""}), response(std::move(value)) {{}}
Response{response.status} response;
}};
'''
    successes = [r for r in op.responses if r.status < 300]
    if not successes or (len(successes) == 1 and successes[0].media is None):
        result = 'void'
    elif len(successes) == 1:
        result = f'Response{successes[0].status}'
    else:
        result = 'std::variant<' + ', '.join(f'Response{r.status}' for r in successes) + '>'
    text += f'using Response = {result};\n'
    auth_argument = ', std::string_view access_token' if op.authenticated else ''
    text += f'inline platform::http::Request Encode(const Request& request{auth_argument}) {{\nplatform::http::Request raw;\n'
    if op.authenticated:
        text += 'if (access_token.empty() || access_token.find_first_of("\\r\\n") != std::string_view::npos) throw std::invalid_argument("Invalid bearer token"); raw.bearer_token = std::string(access_token);\n'
    text += f'raw.method = {quoted(op.method)}; raw.path = {quoted(op.path)}; raw.path_template = {quoted(op.path)};\n'
    for f in op.params:
        value = f'*request.{f.name}' if f.optional else f'request.{f.name}'
        line = f'Validate_Request_{f.name}({value});\n'
        if f.location == 'path':
            line += f'platform::http::ReplacePath(raw.path, {quoted("{" + f.name + "}")}, platform::http::Scalar({value}));'
        else:
            line += f'raw.query.emplace_back({quoted(f.name)}, platform::http::Scalar({value}));'
        text += f'if (request.{f.name}) {{ {line} }}\n' if f.optional else line + '\n'
    if op.body:
        line = 'raw.body = ToJson(' + ('*request.body' if not op.body.required else 'request.body') + '); raw.content_type = "application/json";'
        text += (f'if (request.body) {{ {line} }}' if not op.body.required else line) + '\n'
    text += 'return raw;\n}\ninline Response Decode(const platform::http::Response& raw) {\ntry {\nswitch (raw.status) {\n'
    for response in op.responses:
        text += f'case {response.status}: {{\n'
        finish = f'throw Error{response.status}{{std::move(value)}};' if response.status >= 300 else ('return;' if result == 'void' else 'return value;')
        if response.body:
            text += json_read(response, finish)
        elif response.media == 'text/plain':
            text += f'platform::http::RequireContentType(raw, "text/plain"); Response{response.status} value{{raw.body}}; {finish}'
        else:
            text += 'if (!raw.body.empty()) throw std::invalid_argument("unexpected body"); '
            if result != 'void' or response.status >= 300:
                text += f'Response{response.status} value; '
            text += finish
        text += '\n}\n'
    text += '''default: throw platform::http::ContractError("Undeclared HTTP response status");
}
} catch (const std::bad_alloc&) { throw;
} catch (const platform::http::ApiError&) { throw;
} catch (const platform::http::ContractError&) { throw;
} catch (const std::exception&) { throw platform::http::ContractError("Invalid HTTP response contract"); }
}
'''
    return text + f'}}  // namespace {op.name}\n'


def client_outputs(service, loader, load):
    result = {}
    for alias, provider in client_specs(service, loader):
        ops = load(provider, public_only=True)
        result[f'include/clients/{alias}/client.hpp'] = render_client(service.name, alias, ops, provider.name)
    return result


def render_client(service, alias, ops, provider=None):
    ns = f'{service}::clients::{alias}'
    code = '''#pragma once
#include <openapi/runtime.hpp>
#include <platform/http_client.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/serialize.hpp>
#include <limits>
#include <optional>
#include <variant>
#include <new>
'''
    if provider:
        code += '#include <configs/keys.hpp>\n#include <userver/dynamic_config/storage/component.hpp>\n'
    from models_render import render_models, bind_operation
    code += render_models(collect_models(ops), ns + '::models', header=False)
    code += f'namespace {ns} {{\n'
    for op in ops:
        code += operation_code('::' + ns + '::' + op.name, bind_operation(op, ns + '::models'))
    code += 'class Client {\npublic:\nexplicit Client(platform::http::Transport& transport) : transport_(&transport) {}\n'
    for op in ops:
        name = op.name
        argument = ', std::string_view access_token' if op.authenticated else ''
        value = ', access_token' if op.authenticated else ''
        code += f'{name}::Response {name}(const {name}::Request& request{argument}) const {{ return {name}::Decode(transport_->Perform({name}::Encode(request{value}))); }}\n'
    code += 'private:\nplatform::http::Transport* transport_;\n};\n'
    settings = ''
    if provider:
        settings = f''', [source = platform::configs::Source{{context.FindComponent<userver::components::DynamicConfig>().GetSource()}}](const std::string& path) {{
            const auto config = source.GetSnapshot().Get(::configs::{http_name(provider)}).Get(path);
            return platform::http::ClientSettings{{config.Timeout, config.MaxRetries}};
        }}'''
    code += f'''class Component final : public userver::components::ComponentBase {{
public:
static constexpr std::string_view kName = "client-{alias}";
Component(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
: ComponentBase(config, context), transport_(config, context{settings}), client_(transport_) {{}}
const Client& GetClient() const {{ return client_; }}
static userver::yaml_config::Schema GetStaticConfigSchema() {{ return platform::http::ClientConfigSchema(); }}
private:
platform::http::HttpTransport transport_;
Client client_;
}};
}}  // namespace {ns}
'''
    return format_cpp(code)
