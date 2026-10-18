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
    model_name: str | None = None
    nullable: bool = False

    @property
    def cpp(self):
        if self.nullable:
            return "std::optional<" + replace(self, nullable=False).cpp + ">"
        if self.model_name:
            return self.model_name if self.model_name.startswith("::") else "models::" + self.model_name
        if self.enum_name:
            return self.enum_name
        if self.kind == 'integer' and self.schema.get('format') == 'int32':
            return 'std::int32_t'
        if self.kind == 'number' and self.schema.get('format') == 'float':
            return 'float'
        return {'string': 'std::string', 'integer': 'std::int64_t', 'number': 'double', 'boolean': 'bool', 'object': 'userver::formats::json::Value', 'array': f'std::vector<{self.item.cpp}>' if self.item else ''}[self.kind]


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
    validate_content_type: bool = True


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
    models: tuple['NamedModel', ...] = ()
    authenticated: bool = False

    @property
    def name(self):
        return ''.join(word[:1].upper() + word[1:] for word in re.split(r'[/_]', self.rel) if word)

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
        if not typ.enum_name or typ.model_name:
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
            result[typ.enum_name] = replace(typ, nullable=False)
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


def parse_type(schema, prefix='Value', registry=None):
    if '$ref' in schema:
        keys(schema, {'$ref'}, 'reference')
    if 'nullable' in schema:
        if not isinstance(schema['nullable'], bool):
            raise ValueError('nullable must be boolean')
        inner = {key: value for key, value in schema.items() if key != 'nullable'}
        null_default = 'default' in inner and inner['default'] is None
        if null_default:
            if not schema['nullable']:
                raise ValueError('Null default requires nullable: true')
            inner.pop('default')
        typ = parse_type(inner, prefix, registry)
        return replace(typ, nullable=schema['nullable'],
                       schema={**typ.schema, **({'default': None} if null_default else {})})
    if '$ref' in schema:
        if registry is None:
            raise ValueError('References require components.schemas')
        return registry.reference(schema)
    kind = schema.get('type')
    if kind == 'object':
        keys(schema, {'type', 'additionalProperties', 'description', 'title'}, 'free-form object')
        if schema.get('additionalProperties') is not True:
            raise ValueError('Object fields require explicit additionalProperties: true; typed nested objects are unsupported')
        return Type('object', schema=schema)
    keys(schema, SCHEMA_KEYS, 'field schema')
    if kind not in {'string', 'integer', 'number', 'boolean', 'array'}:
        raise ValueError(f'Unsupported field type: {kind}')
    item = None
    if kind == 'array':
        item = parse_type(schema.get('items', {}), prefix + '_item', registry)
        if item.kind == 'array':
            raise ValueError('Nested arrays are unsupported')
        if item.kind in {'object', 'record'} and ('default' in schema or schema.get('uniqueItems')):
            raise ValueError('default and uniqueItems for arrays of objects are unsupported')
    elif 'items' in schema:
        raise ValueError('items is only valid for arrays')
    validate_schema({**schema, 'items': item.schema} if item else schema)
    return Type(kind, item, schema, prefix if 'enum' in schema else None)


def parse_body(schema, schemas, required=True, prefix='Schema', registry=None):
    if '$ref' in schema and registry is not None:
        model = registry.get(registry.reference_name(schema))
        while model.alias:
            model = registry.get(model.alias)
        if model.body is None:
            raise ValueError('JSON body must reference an object with properties')
        return replace(model.body, required=required)
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
    return Body(tuple(Field(identifier(n), parse_type(s, n, registry), n in req) for n, s in sorted(props.items())), required, not extra)


@dataclass(frozen=True)
class NamedModel:
    name: str
    type: Type
    body: Body | None = None
    alias: str | None = None


class SchemaRegistry:
    def __init__(self, schemas):
        if not isinstance(schemas, dict):
            raise ValueError('components.schemas must be an object')
        self.schemas = schemas
        self.models = {}
        self.visiting = []

    def reference_name(self, schema):
        keys(schema, {'$ref'}, 'reference')
        ref = schema['$ref']
        prefix = '#/components/schemas/'
        if not isinstance(ref, str) or not ref.startswith(prefix) or ref[len(prefix):] not in self.schemas:
            raise ValueError(f'Unsupported or unresolved reference: {ref}')
        return ref[len(prefix):]

    def reference(self, schema):
        name = self.reference_name(schema)
        model = self.get(name)
        return replace(model.type, model_name=name)

    def name_inline_enum(self, typ, owner):
        if typ.model_name:
            return typ
        if typ.item:
            return replace(typ, item=self.name_inline_enum(typ.item, owner))
        if not typ.enum_name:
            return typ
        enum_name = owner + '_' + typ.enum_name
        if enum_name in self.schemas or enum_name in self.models:
            raise ValueError(f'Conflicting model enum name: {enum_name}')
        enum_type = replace(typ, enum_name=enum_name, nullable=False)
        self.models[enum_name] = NamedModel(enum_name, enum_type)
        return replace(enum_type, model_name=enum_name, nullable=typ.nullable)

    def get(self, name):
        identifier(name)
        if name in {'std', 'userver', 'broker_codegen', 'models', 'ValidateModel', 'ParseModelJson', 'Serialize', 'EnumValue',
                    'ParseEnumScalar', 'ParseEnumJson', 'detail', 'ToJson'}:
            raise ValueError(f'Reserved model name: {name}')
        if name in self.visiting:
            raise ValueError('Recursive schema reference: ' + ' -> '.join([*self.visiting, name]))
        if name in self.models:
            return self.models[name]
        self.visiting.append(name)
        schema = self.schemas[name]
        if not isinstance(schema, dict):
            raise ValueError(f'components.schemas.{name}: expected an object')
        if '$ref' in schema:
            target = self.reference_name(schema)
            source = self.get(target)
            model = NamedModel(name, replace(source.type, model_name=name), source.body, target)
        elif schema.get('type') == 'object' and ('properties' in schema or schema.get('additionalProperties') is False):
            body = parse_body(schema, self.schemas, prefix=name, registry=self)
            fields = tuple(replace(field, type=self.name_inline_enum(field.type, name))
                           for field in body.fields)
            body = replace(body, fields=fields)
            model = NamedModel(name, Type('record', schema=schema, model_name=name), body)
        else:
            if schema.get('nullable'):
                raise ValueError('Nullable components are unsupported; put nullable on a property schema')
            typ = parse_type(schema, name, self)
            model = NamedModel(name, typ)
        self.visiting.pop()
        self.models[name] = model
        return model


def collect_models(operations):
    models = {}
    for op in operations:
        for model in op.models:
            if model.name in models and models[model.name] != model:
                raise ValueError(f'Conflicting components.schemas model: {model.name}')
            models[model.name] = model
    validation_names = set()
    def validation_functions(typ, name):
        validation_names.add('Validate_' + name)
        if typ.nullable:
            validation_functions(replace(typ, nullable=False), name + '_value')
        elif typ.item:
            validation_functions(typ.item, name + '_item')
    for model in models.values():
        if model.body and not model.alias:
            for field in model.body.fields:
                validation_functions(field.type, model.name + '_' + field.name)
    conflicts = validation_names & models.keys()
    if conflicts:
        raise ValueError('Conflicting model validation name: ' + ', '.join(sorted(conflicts)))
    # Реестр каждого файла уже отсортирован по зависимостям; объединяем DAG заново.
    ordered = {}
    def add(name):
        if name in ordered:
            return
        model = models[name]
        def dependencies(typ):
            if typ.model_name and typ.model_name != name:
                add(typ.model_name)
            if typ.item:
                dependencies(typ.item)
        if model.alias:
            add(model.alias)
        elif model.body:
            for field in model.body.fields:
                dependencies(field.type)
        else:
            dependencies(model.type)
        ordered[name] = model
    for name in sorted(models):
        add(name)
    return tuple(ordered.values())


def parse_spec(spec):
    keys(spec, {'openapi', 'info', 'paths', 'components', 'tags', 'servers', 'security'}, 'document')
    if spec.get('openapi') != '3.0.3':
        raise ValueError('Only OpenAPI 3.0.3 is supported')
    keys(spec.get('components', {}), {'schemas', 'securitySchemes'}, 'components')
    schemes = spec.get('components', {}).get('securitySchemes', {})
    if not isinstance(schemes, dict):
        raise ValueError('securitySchemes must be an object')
    for scheme in schemes.values():
        keys(scheme, {'type', 'scheme', 'bearerFormat', 'description'}, 'security scheme')
        if scheme.get('type') != 'http' or scheme.get('scheme') != 'bearer' or scheme.get('bearerFormat', 'JWT') != 'JWT':
            raise ValueError('Only HTTP bearer JWT security is supported')
    def authenticated(value):
        if value == []:
            return False
        if not isinstance(value, list) or len(value) != 1 or not isinstance(value[0], dict) or len(value[0]) != 1:
            raise ValueError('Expected one bearer security requirement')
        name, scopes = next(iter(value[0].items()))
        if name not in schemes or scopes != []:
            raise ValueError('Unknown bearer scheme or unsupported scopes')
        return True
    authenticated(spec.get('security', []))
    schemas = spec.get('components', {}).get('schemas', {})
    registry = SchemaRegistry(schemas)
    # Проверяем также неиспользуемые модели и строим порядок зависимостей.
    for name in sorted(schemas):
        registry.get(name)
    operations = []
    for path, item in spec.get('paths', {}).items():
        if not isinstance(path, str) or not re.fullmatch(r'/(?:[a-zA-Z0-9_{}-]+/?)*', path) or '//' in path or (path != '/' and path.endswith('/')):
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
        methods = set(item) & {'get', 'post', 'put', 'patch', 'delete', 'head', 'options'}
        if len(methods) != 1:
            raise ValueError(f'{path}: one HTTP method per path is required')
        for method, op in item.items():
            if method not in {'get', 'post', 'put', 'patch', 'delete', 'head', 'options'}:
                continue
            keys(op, {'summary', 'description', 'tags', 'parameters', 'requestBody', 'responses', 'deprecated', 'security'}, f'{method} {path}')
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
                    typ = parse_type(p['schema'], name, registry)
                    if typ.kind in {'array', 'object', 'record'} or typ.nullable:
                        raise ValueError('Array/object parameters are unsupported; arrays are supported in JSON bodies')
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
                if not isinstance(rb.get('x-validate-content-type', True), bool):
                    raise ValueError('requestBody.x-validate-content-type must be boolean')
                keys(rb, {'required', 'content', 'description', 'x-validate-content-type'}, 'requestBody')
                if not isinstance(rb.get('required', False), bool):
                    raise ValueError('requestBody.required must be boolean')
                if set(rb['content']) != {'application/json'}:
                    raise ValueError('Request body supports only application/json')
                keys(rb['content']['application/json'], {'schema'}, 'request media')
                body = parse_body(rb['content']['application/json']['schema'], schemas, rb.get('required', False), 'RequestBody', registry)
                body = replace(body, validate_content_type=rb.get('x-validate-content-type', True))
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
                        rb = parse_body(schema, schemas, prefix=f'Response{status}', registry=registry)
                    elif media != 'text/plain' or schema != {'type': 'string'}:
                        raise ValueError(f'Unsupported response media/schema: {media}')
                responses.append(Response(status, media, rb))
            if not responses:
                raise ValueError('At least one response is required')
            operations.append(Operation(path, method.upper(), '/'.join(names) or 'root', tuple(params.values()), body, tuple(sorted(responses, key=lambda r: r.status)), tuple(registry.models.values()), authenticated(op.get('security', spec.get('security', [])))))
    normalized = []
    for operation in operations:
        canonical = {typ.enum_name: tuple(typ.schema['enum']) for typ in enum_types(operation)}

        def normalize_type(typ):
            if typ.item:
                return replace(typ, item=normalize_type(typ.item))
            return replace(typ, enum_values=canonical[typ.enum_name]) if typ.enum_name and not typ.model_name else typ

        def normalize_fields(fields):
            return tuple(replace(field, type=normalize_type(field.type)) for field in fields)

        def normalize_body(body):
            return replace(body, fields=normalize_fields(body.fields)) if body else None

        normalized.append(replace(operation, params=normalize_fields(operation.params),
                                  body=normalize_body(operation.body),
                                  responses=tuple(replace(response, body=normalize_body(response.body))
                                                  for response in operation.responses)))
    validate_operations(normalized)
    return normalized


def validate_operations(operations):
    """Один путь — одна операция; имена и namespaces не должны конфликтовать."""
    collect_models(operations)
    for label, selector in (
        ('path', lambda op: op.path),
        ('URL template', lambda op: re.sub(r'\{[^}]+\}', '{}', op.path)),
        ('C++ namespace', lambda op: op.rel),
        ('handler name', lambda op: op.handler),
        ('client method', lambda op: op.name),
    ):
        seen = {}
        for op in operations:
            value = selector(op)
            if value in seen:
                raise ValueError(f'Colliding {label}: {seen[value]} and {op.path}')
            seen[value] = op.path
    for op in operations:
        if op.name in {'Client', 'Component'}:
            raise ValueError(f'{op.path}: generated client method {op.name} is reserved')
        # Классы и enum родительской ручки не могут стать namespace дочерней.
        reserved = {'models', 'Request', 'RequestBody', 'Response', 'Handler', 'View', 'ToJson', 'Serialize', 'EnumValue', 'ParseEnumScalar', 'ParseEnumJson'}
        reserved.update(f'Response{response.status}' for response in op.responses)
        reserved.update(typ.enum_name for typ in enum_types(op))
        for child in operations:
            if child.rel.startswith(op.rel + '/'):
                segment = child.rel[len(op.rel) + 1:].split('/')[0]
                if segment in reserved:
                    raise ValueError(f'Colliding C++ namespace: {child.path} uses generated name {segment} of {op.path}')
