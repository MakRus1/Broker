"""Стартовые конфиги проходят тот же путь согласования, что и изменения в админке."""
import json
from pathlib import Path
import urllib.request
from config_api import ConfigAdmin

with urllib.request.urlopen('http://configs:8090/configs/status', timeout=10) as response:
    revision = json.load(response)['revision']
if revision == 0:
    admin = ConfigAdmin('http://configs:8092')
    for name, value in json.loads(Path('/local/initial-configs.json').read_text()).items():
        current = admin.current(name)
        admin.publish(name, value, current['version'], 'Начальные значения локального стенда')
print('Начальные конфиги готовы')
