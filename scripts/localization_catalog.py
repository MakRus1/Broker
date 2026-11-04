"""Регистрация исходных текстов релиза без изменения переводов и черновиков."""
import json
from pathlib import Path
import re
import subprocess

from release_catalog_api import request_json, forward_service


def parameters(text):
    return sorted(set(re.findall(r'\{\w+\}', text, flags=re.ASCII)))


def load(root):
    catalogs = {}
    for locale in ('ru', 'en'):
        path = Path(root) / f'frontend/shared/i18n/{locale}.json'
        def unique(pairs):
            result = {}
            for key, value in pairs:
                if key in result:
                    raise ValueError('Повторяющийся ключ: ' + key)
                result[key] = value
            return result
        catalogs[locale] = json.loads(path.read_text(), object_pairs_hook=unique)
    return validate(catalogs)


def validate(catalogs):
    if set(catalogs) != {'ru', 'en'} or any(not isinstance(catalogs[x], dict) for x in catalogs):
        raise ValueError('Ожидаются каталоги ru и en')
    if not catalogs['ru'] or catalogs['ru'].keys() != catalogs['en'].keys():
        raise ValueError('Наборы ключей ru/en должны совпадать и быть непустыми')
    if len(catalogs['ru']) > 10000:
        raise ValueError('Слишком много ключей')
    for key in catalogs['ru']:
        if not re.fullmatch(r'[A-Za-z0-9_.-]{1,200}', key):
            raise ValueError('Некорректный ключ: ' + key)
        for locale in ('ru', 'en'):
            text = catalogs[locale][key]
            if not isinstance(text, str) or not text or len(text.encode()) > 10000:
                raise ValueError('Некорректный исходный текст: ' + key)
        if parameters(catalogs['ru'][key]) != parameters(catalogs['en'][key]):
            raise ValueError('Не совпадают параметры ru/en: ' + key)
    if len(json.dumps(catalogs, ensure_ascii=False).encode()) > 2 * 1024 * 1024:
        raise ValueError('Каталог превышает 2 МБ')
    return catalogs



def check_compatibility(root, base):
    root = Path(root)
    current = load(root)
    subprocess.run(['git', 'cat-file', '-e', base + '^{commit}'], cwd=root,
                   check=True, capture_output=True)
    errors = []
    for locale in ('ru', 'en'):
        result = subprocess.run(
            ['git', 'show', f'{base}:frontend/shared/i18n/{locale}.json'],
            cwd=root, text=True, capture_output=True,
        )
        if result.returncode:
            # Первое добавление JSON-каталога в репозиторий.
            continue
        previous = json.loads(result.stdout)
        for key in sorted(previous.keys() & current[locale].keys()):
            old = parameters(previous[key])
            new = parameters(current[locale][key])
            if old != new:
                errors.append(f'{locale}: {key}: {old} -> {new}')
    if errors:
        raise ValueError('Изменены параметры существующих ключей. Создайте новые ключи:\n' + '\n'.join(errors))



def register(base_url, token_url, secret, catalogs):
    validate(catalogs)
    access = request_json(token_url, {'grant_type': 'client_credentials',
        'client_id': 'localization-publisher', 'client_secret': secret}, form=True)
    body = {locale: [{'key': key, 'value': text} for key, text in sorted(values.items())]
            for locale, values in catalogs.items()}
    return request_json(base_url + '/api/internal/localization/sources/upsert', body, access['access_token'])


def configure_cluster(kubectl, catalogs, secret):
    with forward_service(kubectl, 'keycloak') as keycloak, forward_service(kubectl, 'localization') as localization:
        return register(localization, keycloak + '/realms/broker/protocol/openid-connect/token', secret, catalogs)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-base')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.check_base:
        check_compatibility(root, args.check_base)
        print('Параметры переводов совместимы с базовой ревизией')
    else:
        from urllib.request import urlopen
        import time
        secrets = json.loads((root / '.local/auth-secrets.json').read_text())
        for attempt in range(60):
            try:
                with urlopen('http://localization:8182/ping/ready', timeout=2):
                    break
            except OSError:
                if attempt == 59:
                    raise RuntimeError('Localization не готов к регистрации') from None
                time.sleep(1)
        register('http://localization:8180',
                 'http://keycloak:8080/realms/broker/protocol/openid-connect/token',
                 secrets['localization-publisher-secret'], load(root))
