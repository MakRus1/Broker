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


@pytest.fixture(scope='session')
def config_service_defaults(config_service_defaults):
    # Общие значения задаются до штатных маркеров config, а не поверх них.
    return {**config_service_defaults, 'BROKER_HTTP_CLIENT_CONFIG': {
        '**default**': {'timeout': 100, 'max_retries': 0}}}


@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Testsuite ожидает живость: готовность может намеренно отсутствовать в тесте.
    return monitor_baseurl.rstrip('/') + '/ping'


@pytest.fixture(name='service_client')
async def _fresh_config_client(service_client):
    # Testsuite disables periodic updates. Refresh before every HTTP test so
    # the five-second readiness policy does not depend on the suite duration.
    # Tests of expiration can still advance time after this preparation.
    await service_client.invalidate_caches(cache_names=['dynamic-config-client-updater'])
    return service_client
