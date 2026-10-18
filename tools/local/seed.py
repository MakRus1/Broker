"""Первый запуск публикует стартовые значения; существующие ревизии сохраняются."""

import json
from pathlib import Path
import urllib.request


def request(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    with urllib.request.urlopen(
        urllib.request.Request(
            "http://configs:8092" + path, data=data, headers={"Content-Type": "application/json"}
        ),
        timeout=10,
    ) as response:
        return json.load(response)


# Публикация CAS не перезаписывает изменения, сделанные пользователем между чтением и записью.
with urllib.request.urlopen("http://configs:8090/configs/status", timeout=10) as response:
    revision = json.load(response)["revision"]
if revision == 0:
    values = json.loads(Path("/local/initial-configs.json").read_text())
    request("/configs/publish", {"expected_revision": 0, "configs": values})
print("Начальные конфиги готовы")
