"""Проверяемое подмножество OpenAPI 3.0: неподдерживаемые конструкции отклоняются."""
from dataclasses import dataclass, field as dataclass_field, replace
import keyword
import re
from validation import SCHEMA_KEYS, validate_schema

CPP_KEYWORDS = set('and_eq bitand bitor char8_t char16_t char32_t compl consteval constinit const_cast dynamic_cast not not_eq or or_eq static_assert static_cast xor_eq alignas alignof and asm auto bool break case catch char class concept const constexpr continue co_await co_return co_yield decltype default delete do double else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not nullptr operator or private protected public register reinterpret_cast requires return short signed sizeof static static_assert struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor'.split())


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r'[a-zA-Z][a-zA-Z0-9_]*', value) or '__' in value or value in CPP_KEYWORDS or keyword.iskeyword(value):
        raise ValueError(f'Invalid C++ identifier: {value!r}')
    return value


def keys(obj, allowed, context):
    if not isinstance(obj, dict):
        raise ValueError(f'{context}: expected an object')
    unknown = set(obj) - set(allowed)
    if unknown:
        raise ValueError(f'{context}: unsupported keys {sorted(unknown)}')


@dataclass(frozen=True)
class Type:
    kind: str
    item: 'Type | None' = None
    schema: dict = dataclass_field(default_factory=dict)
    enum_name: str | None = None
    enum_values: tuple | None = None

    @property
    def cpp(self):
        if self.enum_name:
            return self.enum_name
        if self.kind == 'integer' and self.schema.get('format') == 'int32':
            return 'std::int32_t'
        if self.kind == 'number' and self.schema.get('format') == 'float':
            return 'float'
        return {'string': 'std::string', 'integer': 'std::int64_t', 'number': 'double', 'boolean': 'bool', 'array': f'std::vector<{self.item.cpp}>' if self.item else ''}[self.kind]


@dataclass(frozen=True)
class Field:
    name: str
    type: Type
    required: bool
    location: str = 'body'

    @property
    def optional(self):
        return not self.required and 'default' not in self.type.schema

    @property
    def cpp(self):
        return f'std::optional<{self.type.cpp}>' if self.optional else self.type.cpp


@dataclass(frozen=True)
class Body:
    fields: tuple[Field, ...]
    required: bool = True
    strict: bool = False


@dataclass(frozen=True)
class Response:
    status: int
    media: str | None
    body: Body | None


@dataclass(frozen=True)
class Operation:
    path: str
    method: str
    rel: str
    params: tuple[Field, ...]
    body: Body | None
    responses: tuple[Response, ...]
    operation_id: str | None = None

    @property
    def ns(self):
        return '::'.join(self.rel.split('/'))

    @property
    def handler(self):
        return 'handler-' + self.rel.replace('/', '-')


def enum_types(operation):
    result = {}
    fields = list(operation.params)
    if operation.body:
        fields.extend(operation.body.fields)
    for response in operation.responses:
        if response.body:
            fields.extend(response.body.fields)
    for field in fields:
        typ = field.type.item if field.type.item else field.type
        if not typ.enum_name:
            continue
        if typ.enum_name in {'Request', 'RequestBody', 'Response', 'Handler', 'View'} or re.fullmatch(r'Response[0-9]+', typ.enum_name):
            raise ValueError(f'Enum type name collides with generated API: {typ.enum_name}')
        if typ.enum_name in result:
            previous = result[typ.enum_name]
            if replace(previous, enum_name=None).cpp != replace(typ, enum_name=None).cpp:
                raise ValueError(f'Enum {typ.enum_name} has incompatible underlying types')
            values = list(previous.schema['enum'])
            values.extend(value for value in typ.schema['enum'] if value not in values)
            result[typ.enum_name] = replace(previous, schema={**previous.schema, 'enum': values})
        else:
            result[typ.enum_name] = typ
    return tuple(result.values())


def resolve(schema, schemas):
    if '$ref' in schema:
        keys(schema, {'$ref'}, 'reference')
        ref = schema['$ref']
        prefix = '#/components/schemas/'
        if not isinstance(ref, str) or not ref.startswith(prefix) or ref[len(prefix):] not in schemas:
            raise ValueError(f'Unsupported or unresolved reference: {ref}')
        schema = schemas[ref[len(prefix):]]
        if '$ref' in schema:
            raise ValueError('Chained/recursive references are unsupported')
    return schema


def parse_type(schema, prefix='Value'):
    keys(schema, SCHEMA_KEYS, 'field schema')
    kind = schema.get('type')
    if kind not in {'string', 'integer', 'number', 'boolean', 'array'}:
        raise ValueError(f'Unsupported field type: {kind}')
    item = None
    if kind == 'array':
        item = parse_type(schema.get('items', {}), prefix + '_item')
        if item.kind == 'array':
            raise ValueError('Nested arrays are unsupported')
    elif 'items' in schema:
        raise ValueError('items is only valid for arrays')
    validate_schema(schema)
    return Type(kind, item, schema, prefix if 'enum' in schema else None)


def parse_body(schema, schemas, required=True, prefix='Schema'):
    schema = resolve(schema, schemas)
    keys(schema, {'type', 'properties', 'required', 'additionalProperties', 'description', 'title'}, 'object schema')
    if schema.get('type') != 'object':
        raise ValueError('JSON body must be a flat object')
    props = schema.get('properties', {})
    if not isinstance(props, dict):
        raise ValueError('properties must be an object')
    req = schema.get('required', [])
    if not isinstance(req, list) or not all(isinstance(x, str) for x in req) or len(set(req)) != len(req) or not set(req) <= set(props):
        raise ValueError('required must list unique declared properties')
    if set(props) & {'WasProvided', 'MarkProvided'}:
        raise ValueError('Field name collides with input metadata API')
    extra = schema.get('additionalProperties', True)
    if not isinstance(extra, bool):
        raise ValueError('Typed additionalProperties is unsupported')
    return Body(tuple(Field(identifier(n), parse_type(s, n), n in req) for n, s in sorted(props.items())), required, not extra)


def parse_spec(spec):
    keys(spec, {'openapi', 'info', 'paths', 'components', 'tags', 'servers'}, 'document')
    if spec.get('openapi') != '3.0.3':
        raise ValueError('Only OpenAPI 3.0.3 is supported')
    keys(spec.get('components', {}), {'schemas'}, 'components')
    schemas = spec.get('components', {}).get('schemas', {})
    # Проверяем и неиспользуемые схемы, чтобы ограничения не терялись незаметно.
    for schema in schemas.values():
        parse_body(schema, schemas)
    operations = []
    for path, item in spec.get('paths', {}).items():
        if not isinstance(path, str) or not re.fullmatch(r'/(?:[a-zA-Z0-9_{}-]+/?)*', path) or '//' in path:
            raise ValueError(f'Unsupported URL path: {path!r}')
        segments = [p for p in path.split('/') if p]
        placeholders = []
        names = []
        for p in segments:
            if p.startswith('{') and p.endswith('}'):
                name = identifier(p[1:-1])
                placeholders.append(name)
                names.append('by_' + name)
            else:
                names.append(identifier(p.replace('-', '_')))
        keys(item, {'parameters', 'summary', 'description', 'get', 'post', 'put', 'patch', 'delete', 'head', 'options'}, path)
        for method, op in item.items():
            if method not in {'get', 'post', 'put', 'patch', 'delete', 'head', 'options'}:
                continue
            keys(op, {'operationId', 'summary', 'description', 'tags', 'parameters', 'requestBody', 'responses', 'deprecated'}, f'{method} {path}')
            params = {}
            for level in (item.get('parameters', []), op.get('parameters', [])):
                seen = set()
                for p in level:
                    keys(p, {'name', 'in', 'required', 'schema', 'description'}, 'parameter')
                    name = identifier(p['name'])
                    loc = p['in']
                    if loc not in {'path', 'query'} or not isinstance(p.get('required', False), bool):
                        raise ValueError('Only path/query parameters with boolean required are supported')
                    if loc == 'path' and (not p.get('required') or name not in placeholders):
                        raise ValueError('Path parameters must be required and present in URL')
                    typ = parse_type(p['schema'], name)
                    if typ.kind == 'array':
                        raise ValueError('Array parameters are unsupported; arrays are supported in JSON bodies')
                    if (loc, name) in seen:
                        raise ValueError('Duplicate parameter')
                    seen.add((loc, name))
                    params[(loc, name)] = Field(name, typ, p.get('required', False), loc)
            if set(placeholders) != {n for loc, n in params if loc == 'path'} or len(placeholders) != len(set(placeholders)):
                raise ValueError('URL placeholders and path parameters must match exactly')
            if len({p.name for p in params.values()}) != len(params) or any(p.name in {'body', 'WasProvided', 'MarkProvided'} for p in params.values()):
                raise ValueError('Parameter field collision (body is reserved)')
            body = None
            if 'requestBody' in op:
                rb = op['requestBody']
                keys(rb, {'required', 'content', 'description'}, 'requestBody')
                if not isinstance(rb.get('required', False), bool):
                    raise ValueError('requestBody.required must be boolean')
                if set(rb['content']) != {'application/json'}:
                    raise ValueError('Request body supports only application/json')
                keys(rb['content']['application/json'], {'schema'}, 'request media')
                body = parse_body(rb['content']['application/json']['schema'], schemas, rb.get('required', False), 'RequestBody')
            responses = []
            for raw, response in op.get('responses', {}).items():
                if not re.fullmatch(r'[2-5][0-9]{2}', str(raw)):
                    raise ValueError(f'Explicit status 200..599 required, got {raw}')
                status = int(raw)
                keys(response, {'description', 'content'}, 'response')
                content = response.get('content', {})
                media = None
                rb = None
                if content:
                    if len(content) != 1 or status == 204 or method == 'head':
                        raise ValueError('Responses need one media type; 204/HEAD cannot have a body')
                    media = next(iter(content))
                    keys(content[media], {'schema'}, 'response media')
                    schema = content[media]['schema']
                    if media == 'application/json':
                        rb = parse_body(schema, schemas, prefix=f'Response{status}')
                    elif media != 'text/plain' or schema != {'type': 'string'}:
                        raise ValueError(f'Unsupported response media/schema: {media}')
                responses.append(Response(status, media, rb))
            if not responses:
                raise ValueError('At least one response is required')
            operations.append(Operation(path, method.upper(), '/'.join(names + [('delete_' if method == 'delete' else method)]), tuple(params.values()), body, tuple(sorted(responses, key=lambda r: r.status)), identifier(op["operationId"]) if "operationId" in op else None))
    normalized = []
    for operation in operations:
        canonical = {typ.enum_name: tuple(typ.schema['enum']) for typ in enum_types(operation)}

        def normalize_type(typ):
            if typ.item:
                return replace(typ, item=normalize_type(typ.item))
            return replace(typ, enum_values=canonical[typ.enum_name]) if typ.enum_name else typ

        def normalize_fields(fields):
            return tuple(replace(field, type=normalize_type(field.type)) for field in fields)

        def normalize_body(body):
            return replace(body, fields=normalize_fields(body.fields)) if body else None

        normalized.append(replace(operation, params=normalize_fields(operation.params),
                                  body=normalize_body(operation.body),
                                  responses=tuple(replace(response, body=normalize_body(response.body))
                                                  for response in operation.responses)))
    return normalized
