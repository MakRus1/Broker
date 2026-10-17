# SERVICE_TESTS_BEGIN
import pytest
from testsuite.databases.pgsql import discover
@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    return [service_source_dir / 'postgresql/data']
@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    databases = discover.find_schemas('configs', [service_source_dir / 'postgresql/schemas'])
    return pgsql_local_create(list(databases.values()))
pytest_plugins = ['pytest_userver.plugins.core', 'pytest_userver.plugins.postgresql']
# SERVICE_TESTS_END

@pytest.fixture(scope='session')
async def service_http_ping_url(monitor_baseurl):
    # Готовность проверяется на внутреннем listener до первого запроса testsuite.
    return monitor_baseurl.rstrip('/') + '/ping/ready'
