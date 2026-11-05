#!/usr/bin/env python3
"""Полный HTTP-путь реестра в отдельных сервисах и БД; рабочие БД не меняются."""

import concurrent.futures
import http.cookiejar
import html.parser
import json
import pathlib
import statistics
import subprocess
import time
import urllib.request
import urllib.parse
import urllib.error
import uuid
import tempfile
import yaml

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = pathlib.Path(tempfile.mkdtemp(prefix="broker-critical-bench-"))
PREFIX = "broker-critical-bench-" + uuid.uuid4().hex[:8]
ORG = "11111111-1111-4111-8111-111111111111"
BASE = "http://localhost:14180"
PORT = 14180


def docker(*args, input=None):
    p = subprocess.run(["docker", *args], input=input, text=True, capture_output=True)
    if p.returncode:
        raise RuntimeError(p.stderr[-3000:])
    return p.stdout


def initialize_runtime():
    global image, network, mounts
    base = json.loads(docker("inspect", "broker-local-transportations-1"))[0]
    image = base["Config"]["Image"]
    network = next(iter(base["NetworkSettings"]["Networks"]))
    mounts = []
    for m in base["Mounts"]:
        if m["Destination"] == "/local":
            continue
        mounts += ["-v", f"{m.get('Name') or m['Source']}:{m['Destination']}:ro"]


def sql(db, query):
    return docker(
        "exec",
        "-i",
        PREFIX + "-pg",
        "psql",
        "-h",
        "127.0.0.1",
        "-U",
        "testsuite",
        "-d",
        db,
        "-XAt",
        "-v",
        "ON_ERROR_STOP=1",
        input=query,
    )


def start():
    docker(
        "run",
        "-d",
        "--rm",
        "--name",
        PREFIX + "-pg",
        "--network",
        network,
        "--memory",
        "768m",
        "--cpus",
        "2",
        "--log-opt",
        "max-size=10m",
        "-e",
        "POSTGRES_USER=testsuite",
        "-e",
        "POSTGRES_PASSWORD=testsuite",
        "postgres:16.14-alpine",
    )
    for _ in range(100):
        try:
            sql("postgres", "SELECT 1")
            break
        except RuntimeError:
            time.sleep(0.1)
    for service in ["shipments", "transportations"]:
        sql("postgres", f"CREATE DATABASE {service}_db_1;")
        sql(service + "_db_1", (ROOT / ".local" / f"{service}.sql").read_text())
    sql(
        "shipments_db_1",
        f"""INSERT INTO shipments (organization_id,id,number,customer_id,shipment_date,status,cargo_description,route_segments)
 SELECT '{ORG}',md5('bench-shipment-'||i)::uuid,'BENCH-'||i,'33333333-3333-4333-8333-333333333333',DATE '2025-01-01'+(i%730),'Новая','Benchmark cargo '||i,
 ARRAY[ROW(md5('bench-sea-'||i)::uuid,'sea',NULL,NULL)::shipment_route_segment_v3,ROW(md5('bench-rail-'||i)::uuid,'rail',NULL,NULL)::shipment_route_segment_v3]
 FROM generate_series(1,10000) i; ANALYZE;""",
    )
    sql(
        "transportations_db_1",
        f"""INSERT INTO transportations (organization_id,id,shipment_id,ordinal,container_type_code,container_ownership,container_number,type,hazardous)
 SELECT '{ORG}',md5('bench-transport-'||i)::uuid,md5('bench-shipment-'||(1+(i%10000)))::uuid,1,
 CASE WHEN i%3=0 THEN '20DC' ELSE '40HC' END,CASE WHEN i%2=0 THEN 'SOC' ELSE 'COC' END,
 CASE WHEN i%10=0 THEN NULL ELSE 'BENCH'||lpad(i::text,7,'0') END,
 CASE WHEN i%10=0 THEN 'rail' ELSE 'container' END,i%100=1 FROM generate_series(1,100000) i;
 ANALYZE;""",
    )

    sql(
        "transportations_db_1",
        f"""INSERT INTO shipment_route_segments (organization_id,shipment_id,id,mode,document)
 SELECT '{ORG}',md5('bench-shipment-'||i)::uuid,md5('bench-'||mode||'-'||i)::uuid,mode,
 jsonb_build_object('id',md5('bench-'||mode||'-'||i)::uuid,'type',mode)
 FROM generate_series(1,10000) i CROSS JOIN (VALUES('sea'),('rail')) m(mode);
 INSERT INTO route_segments (organization_id,transportation_id,id,position,document)
 SELECT organization_id,id,md5('bench-rail-'||(1+(number%10000)))::uuid,1,
 jsonb_build_object('id',md5('bench-rail-'||(1+(number%10000)))::uuid,'mode','rail','planned_departure_at','2026-11-05T00:00:00Z') FROM transportations;
 ANALYZE;""",
    )
    for service in ["shipments", "transportations", "web_bff"]:
        config = yaml.safe_load((ROOT / ".local" / f"{service}.yaml").read_text())
        c = config["components_manager"]["components"]
        if service != "web_bff":
            c["postgres-" + service][
                "dbconnection"
            ] = f"postgresql://testsuite:testsuite@{PREFIX}-pg:5432/{service}_db_1"
            c["postgres-" + service].pop("dbconnection#env", None)
        if "client-shipments" in c:
            c["client-shipments"]["base-url"] = f"http://{PREFIX}-shipments:8100"
        if "client-transportations" in c:
            c["client-transportations"][
                "base-url"
            ] = f"http://{PREFIX}-transportations:8110"
        c["otlp-logger"]["sinks"] = {"logs": "default", "tracing": "default"}
        (OUT / f"{service}.yaml").write_text(yaml.safe_dump(config, sort_keys=False))
        ports = {
            "shipments": (19100, 8100),
            "transportations": (19110, 8110),
            "web_bff": (PORT, 8280),
        }[service]
        extra = ["-v", "broker-local_web-ui:/web-ui:ro"] if service == "web_bff" else []
        docker(
            "run",
            "-d",
            "--rm",
            "--name",
            PREFIX + "-" + service,
            "--network",
            network,
            "--memory",
            "512m" if service != "web_bff" else "1g",
            "--cpus",
            "2",
            "--log-opt",
            "max-size=10m",
            *mounts,
            *extra,
            "-v",
            str(OUT) + ":/bench:ro",
            *(["-p", f"127.0.0.1:{PORT}:8280"] if service == "web_bff" else []),
            image,
            f"/workspaces/broker/build-local/services/{service}/{service}",
            "-c",
            f"/bench/{service}.yaml",
        )
    print("Started isolated databases and services; results: " + str(OUT), flush=True)


class Redirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if newurl.startswith("http://localhost:14080/"):
            newurl = newurl.replace("http://localhost:14080/", BASE + "/", 1)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


class LoginParser(html.parser.HTMLParser):
    action = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "form" and a.get("id") == "kc-form-login":
            self.action = a["action"]


jar = http.cookiejar.CookieJar(
    policy=http.cookiejar.DefaultCookiePolicy(secure_protocols=("https", "http"))
)
opener = urllib.request.build_opener(
    urllib.request.HTTPCookieProcessor(jar), Redirect()
)


def login():
    for _ in range(60):
        try:
            page = opener.open(BASE + "/auth/login", timeout=5)
            content = page.read().decode()
            break
        except (OSError, urllib.error.HTTPError):
            time.sleep(0.5)
    else:
        raise RuntimeError("BFF did not start")
    parser = LoginParser()
    parser.feed(content)
    if parser.action:
        realm = json.loads((ROOT / ".local/broker-realm.json").read_text())
        user = next(u for u in realm["users"] if u["username"] == "author")
        password = next(
            c["value"] for c in user["credentials"] if c["type"] == "password"
        )
        target = urllib.parse.urlparse(parser.action)
        if target.hostname != "localhost" or target.port != 18888:
            raise RuntimeError("Login action outside the local Keycloak")
        data = urllib.parse.urlencode(
            {"username": "author", "password": password, "credentialId": ""}
        ).encode()
        try:
            opener.open(parser.action, data=data, timeout=20).read()
        except urllib.error.HTTPError as error:
            raise RuntimeError(
                f"Login HTTP {error.code}: "
                + __import__("re").sub("<[^>]+>", " ", error.read().decode())[-2000:]
            ) from None
    response = opener.open(BASE + "/api/session", timeout=10)
    session = json.load(response)
    (OUT / "session-shape.json").write_text(json.dumps(list(session)))
    return session


SESSION = None


# Эталонные запросы не меняют данные. Все POST проходят реальную проверку CSRF.
def post(path, body):
    data = json.dumps(body).encode()
    request = urllib.request.Request(
        BASE + path,
        data=data,
        headers={
            "Content-Type": "application/json",
            "Origin": "http://localhost:14080",
            "X-CSRF-Token": SESSION.get("csrf_token", SESSION.get("csrf", "")),
        },
    )
    t = time.perf_counter()
    try:
        response = opener.open(request, timeout=20)
        raw = response.read()
        return {
            "ms": (time.perf_counter() - t) * 1000,
            "status": response.status,
            "bytes": len(raw),
            "data": json.loads(raw),
        }
    except urllib.error.HTTPError as error:
        return {
            "ms": (time.perf_counter() - t) * 1000,
            "status": error.code,
            "error": error.read().decode()[:300],
        }


def bench():
    cases = {
        "first": {"sort": "container_number"},
        "type_sort": {"sort": "type"},
        "container_sort": {"sort": "container_type"},
        "short_contains": {
            "sort": "container_number",
            "filters": [{"field": "container_number", "query": "1"}],
        },
        "absent": {
            "sort": "container_number",
            "filters": [{"field": "container_number", "query": "not-found"}],
        },
        "combined": {
            "sort": "container_number",
            "filters": [
                {"field": "container_type", "values": ["40HC"]},
                {"field": "hazardous", "values": ["true"]},
            ],
        },
    }
    report = {}
    for name, body in cases.items():
        samples = [
            post("/api/v1/transportations/list", dict(body, limit=50))
            for _ in range(20)
        ]
        report[name] = {
            "first_sample_ms": samples[0]["ms"],
            "p50_ms": statistics.median(x["ms"] for x in samples[1:]),
            "p95_ms": sorted(x["ms"] for x in samples[1:])[-1],
            "statuses": list(set(x["status"] for x in samples)),
            "bytes": samples[-1].get("bytes"),
            "rows": len(samples[-1].get("data", {}).get("items", [])),
        }
        if samples[-1]["status"] != 200:
            report[name]["error"] = samples[-1].get("error")
        print(name, report[name], flush=True)
    (OUT / "http-results.json").write_text(json.dumps(report, indent=2))
    if any(item["statuses"] != [200] for item in report.values()):
        raise RuntimeError(
            "Нагрузочный сценарий вернул ошибки; смотрите http-results.json"
        )
    return report


def cleanup():
    for service in ["web_bff", "transportations", "shipments", "pg"]:
        subprocess.run(
            ["docker", "rm", "-f", PREFIX + "-" + service], capture_output=True
        )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Изолированные PostgreSQL → сервисы → BFF: 100000 перевозок, 10000 поставок. Нужен запущенный local-up."
    )
    parser.add_argument("--concurrency", type=int, default=10)
    parser.add_argument("--seconds", type=int, default=15)
    parser.add_argument("--port", type=int, default=14180)
    args = parser.parse_args()
    PORT = args.port
    BASE = f"http://localhost:{PORT}"
    if not 1 <= args.concurrency <= 50 or not 1 <= args.seconds <= 300:
        parser.error("concurrency: 1–50, seconds: 1–300")
    try:
        initialize_runtime()
        start()
        SESSION = login()
        bench()
        deadline = time.perf_counter() + args.seconds
        started = time.perf_counter()

        def worker(_):
            samples = []
            while time.perf_counter() < deadline:
                cases = [
                    (
                        "/api/v1/transportations/list",
                        {"limit": 50, "sort": "container_number"},
                    ),
                    (
                        "/api/v1/transportations/list",
                        {"limit": 50, "sort": "container_type"},
                    ),
                    (
                        "/api/v1/transportations/list",
                        {
                            "limit": 50,
                            "sort": "container_number",
                            "filters": [{"field": "container_number", "query": "1"}],
                        },
                    ),
                    (
                        "/api/v1/transportations/list",
                        {
                            "limit": 50,
                            "sort": "container_number",
                            "route_filters": [
                                {"filters": [{"field": "mode", "values": ["rail"]}]}
                            ],
                        },
                    ),
                    (
                        "/api/v1/transportations/filter-values",
                        {"registry": {}, "field": "container_number", "limit": 40},
                    ),
                ]
                result = post(*cases[(_ + len(samples)) % len(cases)])
                samples.append((result["ms"], result["status"]))
            return samples

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=args.concurrency
        ) as pool:
            samples = sum(list(pool.map(worker, range(args.concurrency))), [])
        elapsed = time.perf_counter() - started
        values = sorted(x[0] for x in samples)
        result = {
            "requests": len(values),
            "rps": len(values) / elapsed,
            "p50_ms": statistics.median(values),
            "p95_ms": values[int(0.95 * (len(values) - 1))],
            "p99_ms": values[int(0.99 * (len(values) - 1))],
            "statuses": {
                str(status): sum(x[1] == status for x in samples)
                for status in set(x[1] for x in samples)
            },
        }
        (OUT / "load-results.json").write_text(json.dumps(result, indent=2))
        print(json.dumps(result, indent=2))
        if result["statuses"] != {"200": len(samples)}:
            raise RuntimeError(
                "Нагрузочный сценарий вернул ошибки; смотрите load-results.json"
            )
    finally:
        cleanup()
