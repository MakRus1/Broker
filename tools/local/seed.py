"""Стартовые конфиги проходят тот же путь согласования, что и изменения в админке."""
import json
import time
from pathlib import Path
import urllib.request
from config_api import ConfigAdmin

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
    for name, value in json.loads(Path('/local/initial-configs.json').read_text()).items():
        current = admin.current(name)
        admin.publish(name, value, current['version'], 'Начальные значения локального стенда')
print('Начальные конфиги готовы')
