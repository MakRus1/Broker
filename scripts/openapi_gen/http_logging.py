"""Явная политика логирования тел HTTP из service.yaml."""

from fnmatch import fnmatchcase

DEFAULT_LIMIT = 32768


def policy(spec):
    if 'http_logging' not in spec:
        return None
    value = spec['http_logging']
    if not isinstance(value, dict) or set(value) - {
        'paths',
        'request_body_limit',
        'response_body_limit',
    }:
        raise ValueError(
            'service.yaml: http_logging поддерживает paths/request_body_limit/response_body_limit'
        )
    paths = value.get('paths')
    if (
        not isinstance(paths, list)
        or not paths
        or any(not isinstance(path, str) or not path.startswith('/') for path in paths)
    ):
        raise ValueError(
            'service.yaml: http_logging.paths — непустой список абсолютных путей/масок'
        )
    if len(set(paths)) != len(paths):
        raise ValueError('service.yaml: http_logging.paths содержит повторения')
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
    if settings is None or not any(fnmatchcase(path, pattern) for pattern in settings[0]):
        return 0, 0
    return settings[1], settings[2]
