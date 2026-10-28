"""Cookie и CSRF сохраняются на новых административных маршрутах."""

import pytest

ORG = '11111111-1111-4111-8111-111111111111'


@pytest.mark.parametrize(
    'path,body,result,status',
    [
        (
            '/api/organizations/me',
            None,
            {'platform_admin': True, 'organizations': [], 'managed_organization_ids': []},
            200,
        ),
        ('/api/admin/organizations/list', {}, {'items': []}, 200),
        (
            '/api/admin/organizations/create',
            {'id': ORG, 'name': 'Компания'},
            {'organization': {'id': ORG, 'name': 'Компания', 'enabled': True, 'revision': 1}},
            201,
        ),
    ],
)
async def test_organization_proxy(browser, mockserver, path, body, result, status):
    @mockserver.handler(path)
    def upstream(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        if body is not None:
            sent = request.json
            for key, value in body.items():
                assert sent[key] == value
        return mockserver.make_response(status=status, json=result)

    await browser.sign_in()
    response = await browser.get(path) if body is None else await browser.post(path, body)
    assert response.status == status, response.text
    assert response.json() == result


async def test_member_changes_require_csrf(browser):
    await browser.sign_in()
    response = await browser.post(
        '/api/admin/organizations/members/add',
        {
            'organization_id': ORG,
            'username': 'employee',
            'role': 'operator',
        },
        headers={'X-CSRF-Token': 'invalid'},
    )
    assert response.status == 403


async def test_organization_admin_has_no_implicit_config_permissions(browser, oidc, mockserver):
    oidc['claims']['resource_access'] = {'broker-logistics': {'roles': []}}

    @mockserver.json_handler('/api/organizations/me')
    def own_organization(request):
        return {
            'platform_admin': False,
            'organizations': [{'id': ORG, 'name': 'Своя компания', 'enabled': True, 'revision': 1}],
            'managed_organization_ids': [ORG],
        }

    session = await browser.sign_in()
    assert session['permissions'] == []
    response = await browser.get('/api/organizations/me')
    assert response.status == 200
    assert response.json()['managed_organization_ids'] == [ORG]


@pytest.mark.parametrize('action', ['get', 'update'])
async def test_shipment_rules_proxy_preserves_cookie_csrf_and_token(browser, mockserver, action):
    rules = {
        'number_template': 'ABC{sequence}',
        'reset_number_yearly': False,
        'statuses': ['Новая'],
        'route_types': ['sea'],
        'planned_containers': True,
        'container_types': ['40HC'],
    }
    body = {'organization_id': ORG}
    if action == 'update':
        body.update(expected_revision=0, rules=rules)
    result = {
        'revision': 1,
        'rules': rules,
        'allowed_route_types': [{'code': 'sea', 'name': 'Море'}],
        'allowed_container_types': ['40HC'],
        'planned_containers_allowed': True,
    }
    path = '/api/admin/shipments/settings/' + action

    @mockserver.json_handler(path)
    def upstream(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json == body
        return result

    await browser.sign_in()
    denied = await browser.post(path, body, headers={'X-CSRF-Token': 'invalid'})
    assert denied.status == 403
    assert upstream.times_called == 0
    response = await browser.post(path, body)
    assert response.status == 200, response.text
    assert response.json() == result

@pytest.mark.parametrize('path,filters',[
    ('/api/admin/organizations/list',{'sort':'name','enabled':False}),
    ('/api/admin/organizations/members/list',{'organization_id':ORG,'sort':'display_name','role':'viewer','enabled':True}),
    ('/api/admin/railway-stations/list',{'sort':'name','country_code':'RU'}),
    ('/api/admin/route-locations/list',{'sort':'code','kind':'airport','country_code':'RU'}),
])
async def test_registry_proxy_preserves_filters_and_cursor(browser,mockserver,path,filters):
    identity=ORG
    cursor={'id':identity,'scope':'0'*64,'value':'alpha'}
    body={**filters,'query':'часть','limit':5,'descending':True,'cursor':cursor}
    @mockserver.json_handler(path)
    def upstream(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json==body
        return {'items':[],'next_cursor':cursor}
    await browser.sign_in()
    response=await browser.post(path,body)
    assert response.status==200,response.text
    assert response.json()=={'items':[],'next_cursor':cursor}
