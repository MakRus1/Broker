"""Мок центральных платформенных назначений; подпись JWT проверяет сервис."""

import base64
import json


def install(mockserver):
    state = {'permissions': None, 'unavailable': False}

    @mockserver.handler('/api/platform/access')
    def check(request):
        if state['unavailable']:
            return mockserver.make_response(status=503)
        token = request.headers['Authorization'].split(' ', 1)[1]
        payload = json.loads(base64.urlsafe_b64decode(token.split('.')[1] + '=='))
        permissions = state['permissions']
        if permissions is None:
            permissions = (
                payload.get('resource_access', {}).get('broker-logistics', {}).get('roles', [])
            )
        permission = request.json['permission']
        if permission != 'authenticated' and permission not in permissions:
            return mockserver.make_response(
                status=403, json={'error': 'permission_denied', 'message': 'Нет доступа'}
            )
        return mockserver.make_response(json={'permissions': permissions, 'revision': 1})

    return state
