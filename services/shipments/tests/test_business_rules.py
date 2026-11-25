"""Правила принадлежат организации и не обходят платформенные ограничения."""

import asyncio
from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION
from test_shipment_card import creation

GET = '/api/admin/shipments/settings/get'
UPDATE = '/api/admin/shipments/settings/update'


async def test_rules_revision_and_defaults_for_new_shipments(api):
    initial = await api.post(GET, {'organization_id': ORGANIZATION})
    assert initial['revision'] == 0
    rules = {
        **initial['rules'],
        'number_template': 'ABC{sequence}/{yyyy}',
        'statuses': ['Новая', 'В работе'],
    }
    changed = await api.post(
        UPDATE, {'organization_id': ORGANIZATION, 'expected_revision': 0, 'rules': rules}
    )
    assert changed['revision'] == 1
    assert changed['rules'] == rules
    assert (await api.post(GET, {'organization_id': ORGANIZATION})) == changed
    shipment = (
        await api.post(
            '/api/v1/shipments/create', creation(shipment_date='2026-01-01'), expected=201
        )
    )['shipment']
    assert shipment['number'] == 'ABC1/2026'
    assert shipment['status'] == 'Новая'
    other = await api.post(GET, {'organization_id': OTHER_ORGANIZATION}, user='other')
    assert other['revision'] == 0
    await api.post(
        UPDATE,
        {'organization_id': ORGANIZATION, 'expected_revision': 0, 'rules': rules},
        expected=409,
    )


@pytest.mark.parametrize(
    'user,organization', [('viewer', ORGANIZATION), ('author', OTHER_ORGANIZATION)]
)
async def test_only_administrator_of_target_organization_can_manage_rules(api, user, organization):
    await api.post(GET, {'organization_id': organization}, user=user, expected=403)
    own = await api.post(GET, {'organization_id': ORGANIZATION})
    await api.post(
        UPDATE,
        {'organization_id': organization, 'expected_revision': 0, 'rules': own['rules']},
        user=user,
        expected=403,
    )


@pytest.mark.parametrize(
    'changes',
    [
        {'number_template': '{unknown}'},
        {'number_template': 'ABC'},
        {'statuses': ['Новая', 'Новая']},
        {'statuses': ['  ']},
        {'route_types': ['unknown']},
        {'container_types': ['unknown']},
    ],
)
async def test_invalid_rules_do_not_change_stored_settings(api, changes):
    initial = await api.post(GET, {'organization_id': ORGANIZATION})
    await api.post(
        UPDATE,
        {
            'organization_id': ORGANIZATION,
            'expected_revision': 0,
            'rules': {**initial['rules'], **changes},
        },
        expected=400,
    )
    assert await api.post(GET, {'organization_id': ORGANIZATION}) == initial


async def test_concurrent_first_save_keeps_one_revision(api):
    initial = await api.post(GET, {'organization_id': ORGANIZATION})

    async def save(prefix):
        return await api.post(
            UPDATE,
            {
                'organization_id': ORGANIZATION,
                'expected_revision': 0,
                'rules': {**initial['rules'], 'number_template': prefix + '{sequence}'},
            },
            expected=(200, 409),
        )

    results = await asyncio.gather(save('A'), save('B'))
    assert sum('revision' in result for result in results) == 1


@pytest.mark.config(
    CONTAINER_CATALOG={
        '20DC': {'name': '20 Dry', 'iso_codes': ['22G1'], 'active': False},
        '40HC': {'name': '40 High Cube', 'iso_codes': ['45G1'], 'active': True},
    },
)
async def test_catalog_hides_inactive_groups_and_preserves_existing_inquiry_plan(api):
    settings = await api.post('/api/v1/shipments/settings', {})
    assert settings['container_types'] == ['40HC']
    old_position = {
        'container_type_code': '20DC',
        'container_ownership': 'COC',
        'planned_quantity': 2,
    }
    shipment = (await api.post(
        '/api/v1/shipments/create', creation(planned_containers=[old_position]), expected=201,
    ))['shipment']
    plan = await api.post('/api/v1/shipment-items/list', {'shipment_id': shipment['id']})
    assert plan['items'][0]['container_type_code'] == '20DC'
    assert plan['items'][0]['planned_quantity'] == 2

    result = await api.post(
        '/api/v1/shipment-items/add',
        {
            **old_position,
            'shipment_id': shipment['id'],
            'expected_revision': shipment['revision'],
            'idempotency_key': str(uuid4()),
        },
        expected=400,
    )
    assert result['error'] == 'invalid_container_type'
