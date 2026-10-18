"""Локальный клиент драфтов; отдельные dev-принципалы автора и подтверждающего."""
import json
import os
from pathlib import Path
import time
import urllib.parse
import urllib.request


class ConfigAdmin:
    def __init__(self, base_url):
        self.base_url = base_url
        state = Path(__file__).resolve().parents[2] / '.local'
        self.credentials = json.loads((state / 'auth-secrets.json').read_text())
        self.token_url = os.environ.get('BROKER_AUTH_TOKEN_URL',
            'http://localhost:18888/realms/broker/protocol/openid-connect/token')
        self.tokens = {}

    def token(self, user):
        token, deadline = self.tokens.get(user, ('', 0))
        if time.monotonic() >= deadline:
            client = 'local-' + user
            data = urllib.parse.urlencode({'grant_type': 'client_credentials', 'client_id': client,
                                           'client_secret': self.credentials[client + '-secret']}).encode()
            with urllib.request.urlopen(urllib.request.Request(self.token_url, data=data), timeout=5) as response:
                value = json.load(response)
            token = value['access_token']
            self.tokens[user] = token, time.monotonic() + value['expires_in'] - 30
        return token

    def request(self, path, body, user='author'):
        with urllib.request.urlopen(urllib.request.Request(
                self.base_url + path, data=json.dumps(body).encode(),
                headers={'Content-Type': 'application/json', 'Authorization': 'Bearer ' + self.token(user)}), timeout=10) as response:
            return json.load(response)

    def current(self, name):
        return self.request('/admin/configs/current', {'config_name': name})

    def publish(self, name, value, expected_version, description):
        draft = self.request('/admin/configs/drafts/create', {
            'config_name': name, 'value': value,
            'expected_config_version': expected_version, 'description': description})
        try:
            return self.request('/admin/configs/drafts/approve', {
                'id': draft['id'], 'expected_version': draft['version']}, user='reviewer')
        except Exception:
            # Не оставляем собственный драфт после неуспешной проверки; чужие драфты не трогаем.
            self.request('/admin/configs/drafts/cancel', {
                'id': draft['id'], 'expected_version': draft['version']})
            raise
