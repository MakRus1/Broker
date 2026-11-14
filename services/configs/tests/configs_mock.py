"""Мок полного снимка Configs для тестов потребителей."""

from datetime import datetime, timezone

import pytest


@pytest.fixture
def mock_configs_service(mockserver, dynamic_config):
    @mockserver.json_handler('/configs-service/configs/values')
    def values(request):
        published = dynamic_config.get_values_unsafe()
        resets = dynamic_config.get_kill_switches_disabled_unsafe()
        ids = request.json.get('ids')
        keys = published if ids is None else ids
        response = {
            'configs': {key: published[key] for key in keys if key in published and key not in resets},
            'revision': 0,
            'updated_at': datetime.now(timezone.utc).isoformat(),
        }
        if ids is not None:
            response['kill_switches_disabled'] = [
                key for key in ids if key not in published or key in resets
            ]
        return response

    @mockserver.json_handler('/configs-service/configs/status')
    def status(_request):
        return {'revision': 0}
