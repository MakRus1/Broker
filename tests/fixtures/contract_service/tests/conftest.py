# SERVICE_TESTS_BEGIN
import pytest
pytest_plugins = ['pytest_userver.plugins.core']
# SERVICE_TESTS_END


@pytest.fixture(name='userver_config_testsuite', scope='session')
def _client_config(userver_config_testsuite, mockserver_info):
    def patch(config, config_vars):
        userver_config_testsuite(config, config_vars)
        config_vars['client-broker-base-url'] = mockserver_info.base_url.rstrip('/')
        config_vars['config-updates-enabled'] = True
    return patch


@pytest.fixture(name='userver_config_http_client', scope='session')
def _client_timeouts(userver_config_http_client):
    def patch(config, config_vars):
        userver_config_http_client(config, config_vars)
        config['components_manager']['components']['http-client-core'].pop('testsuite-timeout', None)
    return patch


@pytest.fixture(autouse=True)
def http_dynamic_settings(dynamic_config):
    dynamic_config.set_values({'BROKER_HTTP_CLIENT_CONFIG': {
        '**default**': {'timeout': 100, 'max_retries': 0}}})
