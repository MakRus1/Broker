async def test_preferences_proxy_keeps_organization_and_revision(browser, mockserver):
    await browser.sign_in()
    body = {
        "organization_id": "11111111-1111-4111-8111-111111111111",
        "key": "shipments:columns:v1",
        "value": {"value": ["number"]},
        "expected_revision": 3,
    }

    @mockserver.json_handler("/api/v1/preferences/save")
    def save(request):
        assert request.json == body
        assert request.headers["Authorization"].startswith("Bearer ")
        return {
            "preference": {"key": body["key"], "value": body["value"], "revision": 4}
        }

    response = await browser.post("/api/preferences/save", body)
    assert response.status == 200
    assert response.json()["preference"]["revision"] == 4
    assert save.times_called == 1


async def test_role_registry_defaults_proxy(browser, mockserver):
    await browser.sign_in()
    organization = "11111111-1111-4111-8111-111111111111"
    item = {
        "role": "viewer",
        "registry": "shipments",
        "value": {"columns": [], "query": {}},
        "revision": 1,
    }

    @mockserver.json_handler("/api/organizations/registry-defaults/get")
    def own(request):
        assert request.json == {"organization_id": organization}
        return {"items": [item]}

    @mockserver.json_handler("/api/v1/preferences/list")
    def personal(request):
        return {"preferences": []}

    @mockserver.json_handler("/api/organizations/registry-defaults/list")
    def roles(request):
        assert request.headers["Authorization"].startswith("Bearer ")
        return {"items": [item], "roles": [{"id": "viewer", "name": "Наблюдатель"}]}

    @mockserver.json_handler("/api/organizations/registry-defaults/save")
    def save(request):
        assert request.json == {
            "organization_id": organization,
            "role": "viewer",
            "registry": "shipments",
            "value": item["value"],
            "expected_revision": 0,
        }
        return {"item": item}

    response = await browser.post(
        "/api/preferences/list", {"organization_id": organization}
    )
    assert response.status == 200
    assert response.json() == {"preferences": [], "role_defaults": [item]}
    response = await browser.post(
        "/api/preferences/roles/list", {"organization_id": organization}
    )
    assert response.status == 200
    assert response.json()["items"] == [item]
    response = await browser.post(
        "/api/preferences/roles/save",
        {
            "organization_id": organization,
            "role": "viewer",
            "registry": "shipments",
            "value": item["value"],
            "expected_revision": 0,
        },
    )
    assert response.status == 200
    assert response.json()["item"] == item
