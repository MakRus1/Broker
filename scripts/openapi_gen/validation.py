"""Проверка ограничений схемы и значений default до генерации C++."""
import base64
import datetime
import math
import re
import re2
from functools import lru_cache

SCHEMA_KEYS = {'type', 'items', 'description', 'title', 'enum', 'format', 'default',
               'minimum', 'maximum', 'exclusiveMinimum', 'exclusiveMaximum', 'multipleOf',
               'minLength', 'maxLength', 'pattern', 'minItems', 'maxItems', 'uniqueItems'}
FORMATS = {'string': {'uuid', 'date', 'date-time', 'byte', 'password'},
           'integer': {'int32', 'int64'}, 'number': {'float', 'double'}}
DATE_TIME = r'\d{4}-\d{2}-\d{2}[Tt]\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:[Zz]|[+-]\d{2}:\d{2})'


@lru_cache(maxsize=256)
def pattern(expression):
    options = re2.Options()
    options.log_errors = False
    return re2.compile(expression, options=options)


def numeric(value):
    try:
        return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
    except OverflowError:
        return False


def validate_schema(schema):
    kind = schema['type']
    fmt = schema.get('format')
    if fmt is not None and (not isinstance(fmt, str) or fmt not in FORMATS.get(kind, set())):
        raise ValueError(f'Unsupported format {fmt!r} for {kind}')
    groups = {'minimum': {'integer', 'number'}, 'maximum': {'integer', 'number'},
              'exclusiveMinimum': {'integer', 'number'}, 'exclusiveMaximum': {'integer', 'number'},
              'multipleOf': {'integer', 'number'}, 'minLength': {'string'}, 'maxLength': {'string'},
              'pattern': {'string'}, 'minItems': {'array'}, 'maxItems': {'array'}, 'uniqueItems': {'array'}}
    for key, kinds in groups.items():
        if key in schema and kind not in kinds:
            raise ValueError(f'{key} is invalid for {kind}')
    for key in ['minimum', 'maximum', 'multipleOf']:
        if key in schema and not numeric(schema[key]):
            raise ValueError(f'{key} must be finite numeric')
    if 'multipleOf' in schema and schema['multipleOf'] <= 0:
        raise ValueError('multipleOf must be positive')
    for key in ['exclusiveMinimum', 'exclusiveMaximum', 'uniqueItems']:
        if key in schema and not isinstance(schema[key], bool):
            raise ValueError(f'{key} must be boolean in OpenAPI 3.0')
    for key, bound in [('exclusiveMinimum', 'minimum'), ('exclusiveMaximum', 'maximum')]:
        if schema.get(key) and bound not in schema:
            raise ValueError(f'{key} requires {bound}')
    for key in ['minLength', 'maxLength', 'minItems', 'maxItems']:
        if key in schema and (type(schema[key]) is not int or schema[key] < 0):
            raise ValueError(f'{key} must be a non-negative integer')
    for low, high in [('minimum', 'maximum'), ('minLength', 'maxLength'), ('minItems', 'maxItems')]:
        if low in schema and high in schema and schema[low] > schema[high]:
            raise ValueError(f'{low} exceeds {high}')
    if schema.get('minimum') is not None and schema.get('minimum') == schema.get('maximum') and (schema.get('exclusiveMinimum') or schema.get('exclusiveMaximum')):
        raise ValueError('Exclusive bounds leave no valid value')
    if 'pattern' in schema:
        if not isinstance(schema['pattern'], str):
            raise ValueError('pattern must be a string')
        try:
            pattern(schema['pattern'])
        except re2.error as error:
            raise ValueError(f'Unsupported RE2 pattern: {error}') from error
    if 'enum' in schema:
        values = schema['enum']
        if kind == 'array' or not isinstance(values, list) or not values:
            raise ValueError('enum must be a non-empty list of scalar values')
        if any(value in values[:i] for i, value in enumerate(values)):
            raise ValueError('enum values must be unique')
        for value in values:
            validate_value(value, {k: v for k, v in schema.items() if k not in {'enum', 'default'}})
    if 'default' in schema:
        validate_value(schema['default'], schema)


def validate_value(value, schema):
    kind = schema['type']
    if kind == 'string':
        if not isinstance(value, str):
            raise ValueError('Expected string value')
        value.encode('utf-8')
    if kind == 'boolean' and not isinstance(value, bool):
        raise ValueError('Expected boolean value')
    if kind == 'integer':
        bits = 32 if schema.get('format') == 'int32' else 64
        if type(value) is not int or not -(2**(bits-1)) <= value < 2**(bits-1):
            raise ValueError(f'Expected int{bits} value')
    if kind == 'number' and (not numeric(value) or (schema.get('format') == 'float' and abs(value) > 3.402823466e38)):
        raise ValueError('Expected finite numeric value')
    if kind == 'array':
        if not isinstance(value, list):
            raise ValueError('Expected array value')
        for item in value:
            validate_value(item, schema['items'])
    if 'enum' in schema and value not in schema['enum']:
        raise ValueError('Value is outside enum')
    for key, fail in [('minimum', lambda v: value <= v if schema.get('exclusiveMinimum') else value < v),
                      ('maximum', lambda v: value >= v if schema.get('exclusiveMaximum') else value > v)]:
        if key in schema and fail(schema[key]):
            raise ValueError(f'Value violates {key}')
    if 'multipleOf' in schema:
        remainder = math.fmod(value, schema['multipleOf'])
        if not (abs(remainder) <= schema['multipleOf'] * 1e-9 or abs(abs(remainder)-schema['multipleOf']) <= schema['multipleOf'] * 1e-9):
            raise ValueError('Value violates multipleOf')
    if kind in {'string', 'array'}:
        suffix = 'Length' if kind == 'string' else 'Items'
        for prefix, fail in [('min', lambda n: len(value) < n), ('max', lambda n: len(value) > n)]:
            key = prefix + suffix
            if key in schema and fail(schema[key]):
                raise ValueError(f'Value violates {key}')
    if 'pattern' in schema and pattern(schema['pattern']).search(value) is None:
        raise ValueError('Value violates pattern')
    if schema.get('uniqueItems') and any(item in value[:i] for i, item in enumerate(value)):
        raise ValueError('Array items must be unique')
    fmt = schema.get('format')
    if kind != 'string' or not fmt or fmt == 'password':
        return
    if fmt == 'uuid' and not re.fullmatch(r'[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}', value):
        raise ValueError('Invalid UUID')
    if fmt == 'date':
        if not re.fullmatch(r'\d{4}-\d{2}-\d{2}', value):
            raise ValueError('Invalid date')
        datetime.date.fromisoformat(value)
    if fmt == 'date-time':
        if not re.fullmatch(DATE_TIME, value):
            raise ValueError('Invalid date-time')
        datetime.datetime.fromisoformat(value.upper().replace('Z', '+00:00'))
    if fmt == 'byte':
        if not re.fullmatch(r'([A-Za-z0-9+/]{4})*([A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?', value):
            raise ValueError('Invalid base64')
        try:
            base64.b64decode(value, validate=True)
        except (ValueError, UnicodeEncodeError) as error:
            raise ValueError('Invalid base64') from error
