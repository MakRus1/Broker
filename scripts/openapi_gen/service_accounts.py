"""Служебные OAuth-клиенты: единое описание для локального стенда и staging."""

from pathlib import Path
import re
import yaml
from permissions import catalog

NAME = re.compile(r'[a-z][a-z0-9]*(?:[-_][a-z0-9]+)*')


class Loader(yaml.SafeLoader):
    pass


def unique_mapping(loader, node, deep=False):
    result = {}
    for key, value in loader.construct_pairs(node, deep=deep):
        if key in result:
            raise ValueError(f'Повторяющийся ключ: {key}')
        result[key] = value
    return result


Loader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def policy(spec):
    accounts = spec.get('service_accounts', {})
    if not isinstance(accounts, dict):
        raise ValueError('service_accounts: ожидается таблица служебных клиентов')
    for alias, account in accounts.items():
        if not isinstance(alias, str) or not NAME.fullmatch(alias):
            raise ValueError('service_accounts: неверное имя подключения')
        if not isinstance(account, dict) or set(account) - {
            'client_id',
            'audience',
            'permissions',
            'keycloak_roles',
            'config',
        }:
            raise ValueError(f'service_accounts.{alias}: неизвестные поля')
        if not isinstance(account.get('client_id'), str) or not NAME.fullmatch(
            account['client_id']
        ):
            raise ValueError(f'service_accounts.{alias}: требуется client_id')
        if 'audience' in account and (
            not isinstance(account['audience'], str) or not NAME.fullmatch(account['audience'])
        ):
            raise ValueError(f'service_accounts.{alias}: неверный audience')
        permissions = account.get('permissions', [])
        if (
            not isinstance(permissions, list)
            or any(not isinstance(item, str) for item in permissions)
            or len(set(permissions)) != len(permissions)
        ):
            raise ValueError(f'service_accounts.{alias}: permissions — уникальные коды прав')
        if permissions and 'audience' not in account:
            raise ValueError(f'service_accounts.{alias}: permissions требуют audience')
        roles = account.get('keycloak_roles', {})
        if not isinstance(roles, dict) or any(
            not isinstance(resource, str)
            or not NAME.fullmatch(resource)
            or not isinstance(values, list)
            or not values
            or any(not isinstance(value, str) or not NAME.fullmatch(value) for value in values)
            or len(set(values)) != len(values)
            for resource, values in roles.items()
        ):
            raise ValueError(f'service_accounts.{alias}: неверные keycloak_roles')
        bindings = account.get('config')
        if (
            not isinstance(bindings, dict)
            or not {'client_id', 'client_secret', 'token_url'} <= bindings.keys()
            or set(bindings) - {'client_id', 'client_secret', 'token_url', 'enabled'}
        ):
            raise ValueError(
                f'service_accounts.{alias}: config требует client_id/client_secret/token_url'
            )
        if any(
            not isinstance(value, str) or not NAME.fullmatch(value) for value in bindings.values()
        ) or len(set(bindings.values())) != len(bindings):
            raise ValueError(f'service_accounts.{alias}: неверные привязки config')
    return accounts


def load(root, loader=Loader):
    result, manifests = {}, {}
    root = Path(root)
    permissions = catalog(root, loader)
    for path in sorted((root / 'services').glob('*/service.yaml')):
        spec = yaml.load(path.read_text(), Loader=loader)
        manifests[path.parent.name] = spec
        variables = yaml.load((path.parent / 'configs/config_vars.yaml').read_text(), Loader=loader)
        for alias, account in policy(spec).items():
            client_id = account['client_id']
            if client_id in result:
                raise ValueError(f'Повторяющийся служебный client_id: {client_id}')
            for variable in account['config'].values():
                if variable not in variables:
                    raise ValueError(f'{path}: неизвестная переменная {variable}')
            for code in account.get('permissions', []):
                if code not in permissions or permissions[code]['scope'] != 'service':
                    raise ValueError(f'{path}: неизвестное или не служебное право {code}')
                owner = permissions[code]['owner']
                provider = yaml.load(
                    (root / f'services/{owner}/service.yaml').read_text(), Loader=loader
                )
                auth = provider.get('auth', {})
                if (
                    auth.get('roles-client') != account['audience']
                    or auth.get('service-clients', {}).get(code) != client_id
                ):
                    raise ValueError(f'{path}: {owner} не доверяет клиенту {client_id} для {code}')
            result[client_id] = {**account, 'service': path.parent.name, 'alias': alias}
    for service, spec in manifests.items():
        for code, client_id in spec.get('auth', {}).get('service-clients', {}).items():
            if client_id not in result or code not in result[client_id].get('permissions', []):
                raise ValueError(
                    f'{service}: нет объявления служебного клиента {client_id} для {code}'
                )
    return result


def secret_key(account):
    return account['client_id'] + '-secret'


def variables(accounts, service, secrets, token_url):
    result = {}
    for account in accounts.values():
        if account['service'] != service:
            continue
        values = {
            'client_id': account['client_id'],
            'client_secret': secrets[secret_key(account)],
            'token_url': token_url,
            'enabled': True,
        }
        result.update({variable: values[key] for key, variable in account['config'].items()})
    return result


def provision(api, accounts, secrets):
    """Повторный запуск сохраняет client ID и синхронизирует секрет и прямые роли."""
    subjects = {}
    for account in accounts.values():
        client_id = account['client_id']
        desired = {
            'clientId': client_id,
            'protocol': 'openid-connect',
            'enabled': True,
            'publicClient': False,
            'standardFlowEnabled': False,
            'directAccessGrantsEnabled': False,
            'serviceAccountsEnabled': True,
            'fullScopeAllowed': False,
            'secret': secrets[secret_key(account)],
            'protocolMappers': [],
        }
        if audience := account.get('audience'):
            desired['protocolMappers'].append(
                {
                    'name': 'service-audience',
                    'protocol': 'openid-connect',
                    'protocolMapper': 'oidc-audience-mapper',
                    'consentRequired': False,
                    'config': {
                        'included.client.audience': audience,
                        'access.token.claim': 'true',
                        'id.token.claim': 'false',
                    },
                }
            )
        found = api('clients?clientId=' + client_id)
        if found:
            api('clients/' + found[0]['id'], {**found[0], **desired}, method='PUT')
        else:
            api('clients', desired)
        client = api('clients?clientId=' + client_id)[0]['id']
        subject = api('clients/' + client + '/service-account-user')['id']
        subjects[client_id] = subject
        roles = {key: list(values) for key, values in account.get('keycloak_roles', {}).items()}
        if account.get('permissions'):
            roles.setdefault(account['audience'], []).extend(account['permissions'])
        for resource, names in roles.items():
            target = api('clients?clientId=' + resource)[0]['id']
            existing = {role['name']: role for role in api('clients/' + target + '/roles')}
            for name in names:
                if name not in existing and name in account.get('permissions', []):
                    api('clients/' + target + '/roles', {'name': name})
                    existing[name] = api('clients/' + target + '/roles/' + name)
                if name not in existing:
                    raise ValueError(f'В Keycloak отсутствует внешняя роль {resource}/{name}')
            declared = [existing[name] for name in names]
            api('users/' + subject + '/role-mappings/clients/' + target, declared)
            api('clients/' + client + '/scope-mappings/clients/' + target, declared)
    return subjects
