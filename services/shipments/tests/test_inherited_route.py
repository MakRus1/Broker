"""Поставка защищает заполненные участки и отдаёт маршрут перевозкам."""

from uuid import uuid4
import pytest
from test_shipment_card import creation


async def test_contexts_are_scoped_and_terms_are_current(api):
    stage = {'id': str(uuid4()), 'type': 'sea'}
    shipment = (
        await api.post(
            '/api/v1/shipments/create',
            creation(route_segments=[stage], incoterms='FOB'),
            expected=201,
        )
    )['shipment']
    body = {'shipment_ids': [shipment['id']]}
    response = await api.post(
        '/api/v1/shipments/transportation-contexts', body, roles=['transportations.read']
    )
    assert response['items'][0]['route_segments'] == [stage]
    assert response['items'][0]['incoterms'] == 'FOB'
    assert (
        await api.post(
            '/api/v1/shipments/transportation-contexts',
            body,
            user='other',
            roles=['transportations.read'],
        )
    )['items'] == []


@pytest.mark.parametrize('change', ['remove', 'type', 'reorder'])
async def test_used_stage_cannot_be_removed_or_retyped(api, change):
    sea, road = [{'id': str(uuid4()), 'type': mode} for mode in ['sea', 'road']]
    shipment = (
        await api.post(
            '/api/v1/shipments/create', creation(route_segments=[sea, road]), expected=201
        )
    )['shipment']
    await api.post(
        '/api/v1/shipments/route-segments/register-usage',
        {
            'shipment_id': shipment['id'],
            'expected_revision': 1,
            'segment_ids': [sea['id']],
        },
        roles=['transportations.read', 'transportations.write'],
    )
    changed = (
        [road]
        if change == 'remove'
        else [{**sea, 'type': 'air'}, road] if change == 'type' else [road, sea]
    )
    response = await api.post(
        '/api/v1/shipments/update',
        {
            'shipment_id': shipment['id'],
            'expected_revision': 1,
            'reference': '',
            'route_segments': changed,
        },
        expected=200 if change == 'reorder' else 409,
    )
    if change != 'reorder':
        assert response['error'] == 'route_segment_in_use'


async def test_stale_usage_registration_rejected(api):
    stage = {'id': str(uuid4()), 'type': 'sea'}
    shipment = (
        await api.post('/api/v1/shipments/create', creation(route_segments=[stage]), expected=201)
    )['shipment']
    await api.post(
        '/api/v1/shipments/update',
        {
            'shipment_id': shipment['id'],
            'expected_revision': 1,
            'reference': '',
            'route_segments': [],
        },
    )
    await api.post(
        '/api/v1/shipments/route-segments/register-usage',
        {
            'shipment_id': shipment['id'],
            'expected_revision': 1,
            'segment_ids': [stage['id']],
        },
        roles=['transportations.read', 'transportations.write'],
        expected=409,
    )
