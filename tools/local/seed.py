"""Стартовые конфиги проходят тот же путь согласования, что и изменения в админке."""
import json
from pathlib import Path
import time
import urllib.request

from config_api import ConfigAdmin


def initialize(admin, values):
    for name, value in values.items():
        current = admin.current(name)
        if current['value'] == value:
            continue
        admin.publish(name, value, current['version'], 'Начальные значения локального стенда')


def main():
    with urllib.request.urlopen('http://configs:8090/configs/status', timeout=10) as response:
        revision = json.load(response)['revision']
    if revision == 0:
        admin = ConfigAdmin('http://configs:8092')
        for attempt in range(90):
            try:
                admin.token('author')
                admin.token('reviewer')
                admin.current('SHIPMENT_TERMS_CONFIG')
                break
            except OSError:
                if attempt == 89:
                    raise
                time.sleep(1)
        initialize(admin, json.loads(Path('/local/initial-configs.json').read_text()))
    print('Начальные конфиги готовы')


if __name__ == '__main__':
    main()
