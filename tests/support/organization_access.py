"""Мок центрального доступа. Подпись токена по-прежнему проверяет сервис."""

import base64
import json


def install(mockserver, organizations):
    @mockserver.handler('/organizations/access')
    def access(request):
        token = request.headers['Authorization'].split(' ', 1)[1]
        payload = json.loads(base64.urlsafe_b64decode(token.split('.')[1] + '=='))
        organization = organizations.get(payload['sub'])
        roles = payload.get('resource_access', {}).get('broker-logistics', {}).get('roles', [])
        body = request.json
        if not organization or body.get('organization_id', organization) != organization or body['permission'] not in roles:
            return mockserver.make_response(status=403, json={'error': 'forbidden', 'message': 'Нет доступа'})
        return mockserver.make_response(json={'organization_id': organization, 'permissions': roles})
