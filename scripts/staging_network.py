"""Проверка адресов односерверного staging до изменения кластера."""

import ipaddress
import json
import subprocess


def validate_addresses(interfaces, nodes, slices):
    local = {
        address['local']
        for interface in interfaces
        for address in interface.get('addr_info', [])
        if address.get('family') == 'inet'
        and not ipaddress.ip_address(address['local']).is_loopback
    }
    node_items = nodes.get('items', [])
    if len(node_items) != 1:
        raise ValueError('Этот staging рассчитан на одну VM; ожидается ровно один узел K3s')
    advertised = {
        address['address']
        for node in node_items
        for address in node.get('status', {}).get('addresses', [])
        if address['type'] == 'InternalIP' and ipaddress.ip_address(address['address']).version == 4
    }
    endpoints = {
        address
        for item in slices.get('items', [])
        if item.get('addressType') == 'IPv4'
        for endpoint in item.get('endpoints', [])
        for address in endpoint.get('addresses', [])
    }
    if not advertised or not endpoints or not (advertised | endpoints) <= local:
        raise ValueError(
            'Адреса K3s не соответствуют текущим адресам VM. '
            f'VM: {sorted(local)}, узел: {sorted(advertised)}, API: {sorted(endpoints)}. '
            'После смены IP выполните на VM sudo systemctl restart k3s, '
            'дождитесь Ready и повторите деплой.'
        )


def check_network(context):
    def read(*command):
        return json.loads(subprocess.check_output(command, text=True))

    validate_addresses(
        read('ip', '-j', '-4', 'address', 'show'),
        read('kubectl', '--context', context, 'get', 'nodes', '-o', 'json'),
        read(
            'kubectl',
            '--context',
            context,
            '-n',
            'default',
            'get',
            'endpointslices',
            '-l',
            'kubernetes.io/service-name=kubernetes',
            '-o',
            'json',
        ),
    )
