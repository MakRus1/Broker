from conftest import ORGANIZATION, OTHER_ORGANIZATION

BASE = "/api/organizations/registry-defaults/"


def setting(**changes):
    return (
        dict(
            organization_id=ORGANIZATION,
            role="viewer",
            registry="shipments",
            value={
                "columns": [{"id": "number", "visible": True, "width": 200}],
                "query": {"sort": "number", "descending": False},
            },
            expected_revision=0,
        )
        | changes
    )


async def test_role_defaults_follow_membership_and_revision(api):
    own = {"organization_id": ORGANIZATION}
    assert (await api.call(BASE + "get", own, user="viewer"))["items"] == []
    item = (await api.call(BASE + "save", setting(), user="org-admin"))["item"]
    assert item["revision"] == 1
    assert (await api.call(BASE + "get", own, user="viewer"))["items"] == [item]
    assert (await api.call(BASE + "get", own, user="org-admin"))["items"] == []
    listed = await api.call(BASE + "list", own, user="org-admin")
    assert listed["items"] == [item]
    assert any(role["id"] == "viewer" for role in listed["roles"])
    await api.call(BASE + "save", setting(), user="org-admin", expected=409)
    changed = await api.call(
        BASE + "save", setting(expected_revision=1), user="org-admin"
    )
    assert changed["item"]["revision"] == 2


async def test_role_defaults_require_server_administrator_check(api):
    await api.call(BASE + "save", setting(), user="viewer", expected=403)
    await api.call(
        BASE + "list", {"organization_id": ORGANIZATION}, user="viewer", expected=403
    )
    await api.call(BASE + "save", setting(), user="other-admin", expected=403)
    await api.call(
        BASE + "get",
        {"organization_id": ORGANIZATION},
        user="other-admin",
        expected=403,
    )
    await api.call(
        BASE + "save", setting(role="missing"), user="org-admin", expected=400
    )
    await api.call(
        BASE + "save", setting(registry="arbitrary-key"), user="org-admin", expected=400
    )
    await api.call(
        BASE + "save", setting(value={"columns": []}), user="org-admin", expected=400
    )
    await api.call(
        BASE + "save",
        setting(value={"columns": [], "query": {}, "note": "x" * 25000}),
        user="org-admin",
        expected=400,
    )
    assert (
        await api.call(
            BASE + "get", {"organization_id": OTHER_ORGANIZATION}, user="other-admin"
        )
    )["items"] == []


async def test_role_default_reset_keeps_revision(api):
    await api.call(BASE + "save", setting(), user="org-admin")
    reset = (
        await api.call(
            BASE + "save", setting(value={}, expected_revision=1), user="org-admin"
        )
    )["item"]
    assert reset["value"] == {}
    assert reset["revision"] == 2
    await api.call(
        BASE + "save", setting(expected_revision=1), user="org-admin", expected=409
    )
    assert (
        await api.call(BASE + "save", setting(expected_revision=2), user="org-admin")
    )["item"]["revision"] == 3
