"""Локальный клиент единственного пути записи конфигов: драфт и подтверждение."""
import json
import urllib.request


class ConfigAdmin:
    def __init__(self, base_url):
        self.base_url = base_url

    def request(self, path, body):
        with urllib.request.urlopen(urllib.request.Request(
                self.base_url + path, data=json.dumps(body).encode(),
                headers={'Content-Type': 'application/json'}), timeout=10) as response:
            return json.load(response)

    def current(self, name):
        return self.request('/admin/configs/current', {'config_name': name})

    def publish(self, name, value, expected_version, description):
        draft = self.request('/admin/configs/drafts/create', {
            'actor': 'local-author', 'config_name': name, 'value': value,
            'expected_config_version': expected_version, 'description': description})
        try:
            return self.request('/admin/configs/drafts/approve', {
                'actor': 'local-reviewer', 'id': draft['id'], 'expected_version': draft['version']})
        except Exception:
            # Не оставляем собственный драфт после неуспешной проверки; чужие драфты не трогаем.
            self.request('/admin/configs/drafts/cancel', {
                'actor': 'local-author', 'id': draft['id'], 'expected_version': draft['version']})
            raise
