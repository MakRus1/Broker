"""Логирование тел HTTP по умолчанию с исключениями из service.yaml."""

from fnmatch import fnmatchcase

DEFAULT_LIMIT = 32768


def policy(spec):
    value = spec.get('http_logging', {})
    if not isinstance(value, dict) or set(value) - {
        'exclude_paths',
        'request_body_limit',
        'response_body_limit',
    }:
        raise ValueError(
            'service.yaml: http_logging поддерживает exclude_paths/request_body_limit/response_body_limit'
        )
    paths = value.get('exclude_paths', [])
    if not isinstance(paths, list) or any(
        not isinstance(path, str) or not path.startswith('/') for path in paths
    ):
        raise ValueError('service.yaml: http_logging.exclude_paths — список абсолютных путей/масок')
    if len(set(paths)) != len(paths):
        raise ValueError('service.yaml: http_logging.exclude_paths содержит повторения')
    limits = []
    for name in ('request_body_limit', 'response_body_limit'):
        limit = value.get(name, DEFAULT_LIMIT)
        if type(limit) is not int or limit < 0:
            raise ValueError(
                f'service.yaml: http_logging.{name} — неотрицательное целое число байт'
            )
        limits.append(limit)
    return paths, *limits


def body_limits(path, settings):
    if settings is None:
        return DEFAULT_LIMIT, DEFAULT_LIMIT
    if any(fnmatchcase(path, pattern) for pattern in settings[0]):
        return 0, 0
    return settings[1], settings[2]
