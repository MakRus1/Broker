"""HTTP-клиенты: независимые DTO, проверка контракта и внедряемый транспорт."""
from pathlib import Path
import re
import yaml
from model import identifier, enum_types
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
        if alias in {'pg_cluster', 'ResolveDeps'}:
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


def json_read(response):
    name = f'Response{response.status}'
    lines = ['platform::http::RequireContentType(raw, "application/json");',
             'const auto json = userver::formats::json::FromString(raw.body);',
             'if (!json.IsObject()) throw std::invalid_argument("object expected");', f'{name} value;']
    if response.body.strict:
        allowed = ' && '.join(f'it.GetName() != {quoted(f.name)}' for f in response.body.fields) or 'true'
        lines.append(f'for (auto it = json.begin(); it != json.end(); ++it) if ({allowed}) throw std::invalid_argument("unknown field");')
    for f in response.body.fields:
        key = quoted(f.name)
        if f.required:
            lines.append(f'if (!json.HasMember({key})) throw std::invalid_argument("missing field");')
        lines.append(f'if (json.HasMember({key})) {{ auto item = broker_codegen::ParseJson<{f.type.cpp}>(json[{key}]); Validate_{name}_{f.name}(item); value.{f.name} = std::move(item); }}')
    return '\n'.join(lines) + '\nreturn value;'


def operation_code(scope, op):
    text = f'namespace {op.operation_id} {{\n'
    text += ''.join(enums(t) for t in enum_types(op))
    if op.body:
        text += dto('RequestBody', op.body.fields, scope)
        text += 'inline std::string ToJson(const RequestBody& value) {\n' + json_write('RequestBody', op.body.fields) + '\n}\n'
    text += declarations('Request', op.params) + 'struct Request {\n' + fields(op.params, scope)
    if op.body:
        text += '\n' + ('RequestBody' if op.body.required else 'std::optional<RequestBody>') + ' body{};'
    text += '\n};\n'
    for response in op.responses:
        name = f'Response{response.status}'
        text += (f'struct {name} {{ std::string body; }};\n' if response.media == 'text/plain' else dto(name, response.body.fields if response.body else (), scope))
    text += 'using Response = std::variant<' + ', '.join(f'Response{r.status}' for r in op.responses) + '>;\n'
    text += 'inline platform::http::Request Encode(const Request& request) {\nplatform::http::Request raw;\n'
    text += f'raw.method = {quoted(op.method)}; raw.path = {quoted(op.path)}; raw.operation = {quoted(op.operation_id)}; raw.url_template = {quoted(op.path)};\n'
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
        if response.body:
            text += json_read(response)
        elif response.media == 'text/plain':
            text += f'platform::http::RequireContentType(raw, "text/plain"); return Response{response.status}{{raw.body}};'
        else:
            text += f'if (!raw.body.empty()) throw std::invalid_argument("unexpected body"); return Response{response.status}{{}};'
        text += '\n}\n'
    text += '''default: throw platform::http::ContractError("Undeclared HTTP response status");
}
} catch (const std::bad_alloc&) { throw;
} catch (const platform::http::ContractError&) { throw;
} catch (const std::exception&) { throw platform::http::ContractError("Invalid HTTP response contract"); }
}
'''
    return text + f'}}  // namespace {op.operation_id}\n'


def client_outputs(service, loader, load):
    result = {}
    for alias, provider in client_specs(service, loader):
        ops = load(provider, public_only=True)
        if any(op.operation_id in {'Client', 'Component', 'transport_'} for op in ops):
            raise ValueError('operationId Client/Component/transport_ зарезервированы клиентским API')
        missing = [f'{op.method} {op.path}' for op in ops if not op.operation_id]
        if missing:
            raise ValueError(f'Клиент {alias}: требуется operationId: ' + ', '.join(missing))
        result[f'include/clients/{alias}/client.hpp'] = render_client(service.name, alias, ops)
    return result


def render_client(service, alias, ops):
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
    code += f'namespace {ns} {{\n'
    for op in ops:
        code += operation_code('::' + ns + '::' + op.operation_id, op)
    code += 'class Client {\npublic:\nexplicit Client(platform::http::Transport& transport) : transport_(&transport) {}\n'
    for op in ops:
        name = op.operation_id
        code += f'{name}::Response {name}(const {name}::Request& request) const {{ return {name}::Decode(transport_->Perform({name}::Encode(request))); }}\n'
    code += 'private:\nplatform::http::Transport* transport_;\n};\n'
    code += f'''class Component final : public userver::components::ComponentBase {{
public:
static constexpr std::string_view kName = "client-{alias}";
Component(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
: ComponentBase(config, context), transport_(config, context), client_(transport_) {{}}
const Client& GetClient() const {{ return client_; }}
static userver::yaml_config::Schema GetStaticConfigSchema() {{ return platform::http::ClientConfigSchema(); }}
private:
platform::http::HttpTransport transport_;
Client client_;
}};
}}  // namespace {ns}
'''
    return format_cpp(code)
