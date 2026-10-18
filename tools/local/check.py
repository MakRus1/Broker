"""Сквозная проверка локального стенда; временный конфиг восстанавливается через CAS."""

import copy
import json
from pathlib import Path
import subprocess
import time
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def request(url, body=None):
    data = None if body is None else json.dumps(body).encode()
    with urllib.request.urlopen(
        urllib.request.Request(
            url,
            data=data,
            headers={"Accept": "application/json", "Content-Type": "application/json"},
        ),
        timeout=5,
    ) as response:
        return response.read()


def wait_for(description, operation, timeout=90, announce=True):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            result = operation()
            if result:
                if announce:
                    print(description + ": OK", flush=True)
                return result
        except (OSError, TimeoutError, ValueError) as error:
            last = error
        time.sleep(2)
    raise RuntimeError(f"{description}: таймаут; {last}")


def trace(trace_id):
    document = json.loads(request(f"http://127.0.0.1:13200/api/traces/{trace_id}"))
    return [
        (batch["resource"], span)
        for batch in document.get("batches", [])
        for scope in batch.get("scopeSpans", [])
        for span in scope.get("spans", [])
    ]


def restart_counts():
    ids = subprocess.check_output(
        [
            "docker",
            "compose",
            "-f",
            str(ROOT / ".local/compose.yaml"),
            "ps",
            "-q",
            "broker",
            "configs",
            "diagnostics",
        ],
        text=True,
    ).split()
    return subprocess.check_output(
        ["docker", "inspect", "--format", "{{.Id}} {{.State.StartedAt}} {{.RestartCount}}", *ids],
        text=True,
    )


def check():
    endpoints = {
        "broker": "http://127.0.0.1:18082/ping/ready",
        "configs": "http://127.0.0.1:18092/ping/ready",
        "diagnostics": "http://127.0.0.1:18085/ping/ready",
        "grafana": "http://127.0.0.1:13000/api/health",
        "loki": "http://127.0.0.1:13100/ready",
        "tempo": "http://127.0.0.1:13200/ready",
    }
    for name, url in endpoints.items():
        wait_for(name, lambda: request(url), timeout=180)
    for plugin in ("loki", "tempo", "prometheus"):
        module = request(f"http://127.0.0.1:13000/public/plugins/{plugin}/module.js")
        assert b"define(" in module or b"System.register" in module, plugin
    scenario = json.loads(request("http://127.0.0.1:18084/scenario", {}))
    assert (
        "local-scenario" in scenario["postgres"] and "local-scenario" in scenario["grpc"]
    ), scenario
    trace_id = scenario["trace_id"]

    def complete_trace():
        spans = trace(trace_id)
        names = [span["name"] for _, span in spans]
        services = {
            attribute["value"].get("stringValue")
            for resource, _ in spans
            for attribute in resource.get("attributes", [])
            if attribute["key"] == "service.name"
        }
        return (
            spans
            if {"broker", "diagnostics"} <= services
            and "POST /scenario" in names
            and "pg_query:insert" in names
            and names.count("handlers.api.HelloService/SayHello") >= 2
            else None
        )

    wait_for("Общая трасса HTTP → PostgreSQL + gRPC", complete_trace)
    query = urllib.parse.urlencode(
        {"query": '{service_name=~"broker|diagnostics"} |= "' + trace_id + '"', "limit": 100}
    )

    def correlated_logs():
        streams = json.loads(request("http://127.0.0.1:13100/loki/api/v1/query_range?" + query))[
            "data"
        ]["result"]
        return {"broker", "diagnostics"} <= {item["stream"]["service_name"] for item in streams}

    wait_for("Логи обоих сервисов с общим trace_id", correlated_logs)
    labels = json.loads(request("http://127.0.0.1:13100/loki/api/v1/labels"))["data"]
    assert "trace_id" not in labels and "span_id" not in labels, labels
    query = urllib.parse.urlencode({"query": 'up{job="services"}'})

    def metrics():
        values = json.loads(request("http://127.0.0.1:19090/api/v1/query?" + query))["data"][
            "result"
        ]
        return len(values) == 3 and all(item["value"][1] == "1" for item in values)

    wait_for("Prometheus получает метрики трех сервисов", metrics)
    before = restart_counts()
    original = json.loads(request("http://127.0.0.1:18090/configs/values", {}))
    modified = copy.deepcopy(original["configs"]["BROKER_HTTP_CLIENT_CONFIG"])
    # Явное значение пути имеет приоритет перед **default**.
    desired_timeout = 2345 if modified.get("/hello_postgres", {}).get("timeout") == 1234 else 1234
    modified["/hello_postgres"] = {"timeout": desired_timeout, "max_retries": 0}
    published = json.loads(
        request(
            "http://127.0.0.1:18092/configs/publish",
            {
                "expected_revision": original["revision"],
                "configs": {"BROKER_HTTP_CLIENT_CONFIG": modified},
            },
        )
    )
    try:

        def applied():
            response = json.loads(request("http://127.0.0.1:18084/scenario", {}))
            spans = wait_for(
                "Доставка трассы", lambda: trace(response["trace_id"]), timeout=30, announce=False
            )
            for _, span in spans:
                if span["name"] == "GET /hello_postgres" and any(
                    attribute["key"] == "timeout_ms"
                    and attribute["value"].get("intValue") == str(desired_timeout)
                    for attribute in span.get("attributes", [])
                ):
                    return True
            return False

        wait_for("Новый timeout применяется без перезапуска", applied)
        assert before == restart_counts(), "Во время проверки сервисы перезапустились"
    finally:
        request(
            "http://127.0.0.1:18092/configs/publish",
            {
                "expected_revision": published["revision"],
                "configs": {
                    "BROKER_HTTP_CLIENT_CONFIG": original["configs"]["BROKER_HTTP_CLIENT_CONFIG"]
                },
            },
        )
    print(f"Трасса: http://localhost:13000/explore (Tempo, trace_id={trace_id})", flush=True)


if __name__ == "__main__":
    check()
