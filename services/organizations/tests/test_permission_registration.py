"""Каталог релиза доступен только доверенному сервисному клиенту."""

PREFIX = '/api/internal/permissions/releases/'


def entry(code='example.read', dependencies=None):
    return dict(code=code, name='Чтение', group='Пример', scope='organization',
                dependencies=dependencies or [])


async def call(api, action, body, expected=200, **kwargs):
    return await api.call(PREFIX + action, body, user='publisher',
                          roles=['organizations.register_permissions'],
                          client='permission-publisher', expected=expected, **kwargs)


async def test_registration_requires_trusted_service_client(api):
    body = dict(owner='example', version='one', entries=[entry()])
    await api.call(PREFIX + 'upsert', body, expected=403)
    await api.call(PREFIX + 'upsert', body, user='viewer',
                   roles=['organizations.register_permissions'], expected=403)
    await api.call(PREFIX + 'upsert', body, user='publisher',
                   client='permission-publisher', expected=403)
    await call(api, 'upsert', body)


async def test_release_is_immutable_and_registration_idempotent(api, pgsql):
    body = dict(owner='example', version='one', entries=[entry()])
    await call(api, 'upsert', body)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT registered_at FROM permission_releases WHERE owner='example'")
        timestamp = cursor.fetchone()[0]
    await call(api, 'upsert', body)
    await call(api, 'upsert', {**body, 'entries': []}, expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT registered_at FROM permission_releases WHERE owner='example'")
        assert cursor.fetchone()[0] == timestamp


async def test_reconcile_is_atomic_and_rollback_reactivates_old_catalog(api, pgsql):
    await call(api, 'upsert', dict(owner='example', version='old', entries=[entry()]))
    await call(api, 'upsert', dict(owner='example', version='new', entries=[]))
    await call(api, 'reconcile', dict(owner='example', versions=['new', 'missing']), expected=409)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT version FROM permission_releases WHERE owner='example' AND active ORDER BY version")
        assert cursor.fetchall() == [('new',), ('old',)]
    await call(api, 'reconcile', dict(owner='example', versions=['new']))
    await call(api, 'activate', dict(owner='example', version='old'))
    await call(api, 'reconcile', dict(owner='example', versions=['old']))
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT version FROM permission_releases WHERE owner='example' AND active")
        assert cursor.fetchall() == [('old',)]
    await call(api, 'activate', dict(owner='example', version='missing'), expected=409)


async def test_invalid_catalog_and_versions_are_rejected(api):
    for entries in [[entry('foreign.read')], [entry(), entry()],
                    [entry(dependencies=['example.read'])], [entry('bad code')],
                    [entry(dependencies=['example.missing'])],
                    [entry(dependencies=['example.write']), entry('example.write', ['example.read'])]]:
        await call(api, 'upsert', dict(owner='example', version='one', entries=entries), expected=400)
    for versions in [[], ['one', 'one']]:
        await call(api, 'reconcile', dict(owner='example', versions=versions), expected=400)


async def test_registration_accepts_reordered_entries(api):
    entries = [entry(), entry('example.write', ['example.read'])]
    await call(api, 'upsert', dict(owner='example', version='ordered', entries=entries))
    await call(api, 'upsert', dict(owner='example', version='ordered', entries=list(reversed(entries))))
