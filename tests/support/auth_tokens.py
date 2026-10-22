"""Подписанные тестовые токены: проверка подписи работает без отдельного Keycloak."""
import base64
import json
import subprocess
import time

ISSUER = 'http://broker-test-issuer'
PERMISSIONS = ['configs.read', 'configs.write', 'configs.approve']


def encode(value):
    return base64.urlsafe_b64encode(value).rstrip(b'=').decode()


class SigningKey:
    def __init__(self, path, kid):
        self.path, self.kid = path, kid
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt',
                        'rsa_keygen_bits:2048', '-out', str(path)], check=True, capture_output=True)
        modulus = subprocess.check_output(['openssl', 'rsa', '-in', str(path), '-noout', '-modulus'])
        self.jwk = {'kty': 'RSA', 'alg': 'RS256', 'use': 'sig', 'kid': kid,
                    'n': encode(bytes.fromhex(modulus.decode().strip().split('=')[1])), 'e': 'AQAB'}

    def token(self, user='author', roles=None, claims=None, header=None):
        now = int(time.time())
        payload = {'iss': ISSUER, 'aud': 'broker-logistics', 'sub': user, 'iat': now, 'exp': now + 300,
                   'typ': 'Bearer', 'resource_access': {'broker-logistics': {'roles': PERMISSIONS if roles is None else roles}}}
        payload.update(claims or {})
        headers = {'alg': 'RS256', 'kid': self.kid, 'typ': 'JWT', **(header or {})}
        message = '.'.join(encode(json.dumps(value, separators=(',', ':')).encode()) for value in [headers, payload])
        signature = subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(self.path)],
                                   input=message.encode(), capture_output=True, check=True).stdout
        return message + '.' + encode(signature)
