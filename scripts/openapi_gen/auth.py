"""Контракт Bearer-авторизации сервиса; инфраструктура генерируется из service.yaml."""

import re


def policy(spec):
    section = spec.get('auth')
    if section is None:
        if 'auth' in spec:
            raise ValueError('service.yaml: auth должен быть объектом')
        return None
    required = {'audience', 'roles-client', 'protected-prefix', 'permissions'}
    if not isinstance(section, dict) or (set(section) - {'organization-access', 'service-clients'}) != required:
        raise ValueError(
            'service.yaml: auth требует audience, roles-client, protected-prefix, permissions'
        )
    if 'organization-access' in section and type(section['organization-access']) is not bool:
        raise ValueError('auth.organization-access должен быть boolean')
    for key in ('audience', 'roles-client'):
        if not isinstance(section[key], str) or not re.fullmatch(
            r'[A-Za-z0-9][A-Za-z0-9_.:-]*', section[key]
        ):
            raise ValueError(f'service.yaml: auth.{key} — непустое имя клиента')
    prefix = section['protected-prefix']
    if (
        not isinstance(prefix, str)
        or prefix == '/'
        or not prefix.startswith('/')
        or not prefix.endswith('/')
        or any(c in prefix for c in '{}*?#')
    ):
        raise ValueError('service.yaml: auth.protected-prefix — не корневой абсолютный префикс с завершающим /')
    permissions = section['permissions']
    if not isinstance(permissions, dict) or not permissions:
        raise ValueError('service.yaml: auth.permissions — непустая таблица путь: право')
    for path, permission in permissions.items():
        if not isinstance(path, str) or not path.startswith(prefix):
            raise ValueError(f'service.yaml: путь {path!r} находится вне auth.protected-prefix')
        if not isinstance(permission, str) or not re.fullmatch(
            r'[A-Za-z0-9][A-Za-z0-9_.:-]*', permission
        ):
            raise ValueError(f'service.yaml: неверное право для {path}')
    clients = section.get('service-clients', {})
    if not isinstance(clients, dict) or any(
        permission not in permissions.values() or not isinstance(client, str)
        or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.:-]*', client)
        for permission, client in clients.items()
    ):
        raise ValueError('auth.service-clients: ожидается таблица разрешение: доверенный OAuth клиент')
    return section


def validate(spec, operations):
    section = policy(spec)
    bearer = {op.path for op in operations if op.authenticated and op.cookie_name is None}
    if section is None:
        if bearer:
            raise ValueError(
                'service.yaml: Bearer-ручки требуют секцию auth: ' + ', '.join(sorted(bearer))
            )
        return
    permissions = section['permissions']
    if missing := bearer - permissions.keys():
        raise ValueError(
            'service.yaml: отсутствуют auth.permissions для ' + ', '.join(sorted(missing))
        )
    if extra := permissions.keys() - bearer:
        raise ValueError(
            'service.yaml: auth.permissions ссылаются на неизвестные или не Bearer-ручки: '
            + ', '.join(sorted(extra))
        )
    for op in operations:
        if op.path.startswith(section['protected-prefix']) and op.path not in bearer:
            raise ValueError(
                f'{op.path}: внутри auth.protected-prefix требуется Bearer security в OpenAPI'
            )
