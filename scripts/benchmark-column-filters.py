#!/usr/bin/env python3
"""Бенчи реальных SQL-построителей: изолированный PostgreSQL, p50/p95 и EXPLAIN."""

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import shlex
import shutil
import statistics
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
ORG = "00000000-0000-0000-0000-000000000001"


def uid(value):
    return str(uuid.UUID(hashlib.md5(str(value).encode()).hexdigest()))


def cases(service):
    if service == "transportations":
        return [
            ("first_page", {"sort": "container_number"}, [], True),
            (
                "next_page",
                {
                    "sort": "container_number",
                    "cursor": {"id": ORG, "scope": "0" * 64, "value": "mscu0100001"},
                },
                ["mscu0100001", ORG],
                True,
            ),
            (
                "combined",
                {
                    "sort": "container_number",
                    "types": ["container"],
                    "container_types": ["20DC"],
                    "hazardous": True,
                },
                [["container"], ["20DC"], True],
                True,
            ),
            (
                "empty_search",
                {"sort": "container_number", "query": "never-matches"},
                ["%never-matches%"],
                True,
            ),
            (
                "empty_values_desc",
                {
                    "sort": "container_number",
                    "descending": True,
                    "filters": [{"field": "container_number", "empty": True}],
                },
                [],
                False,
            ),
            (
                "multi_numbers_20",
                {
                    "sort": "container_number",
                    "filters": [
                        {
                            "field": "container_number",
                            "values": ["MSCU" + str(i).zfill(7) for i in range(1, 21)],
                        }
                    ],
                },
                [["MSCU" + str(i).zfill(7) for i in range(1, 21)]],
                False,
            ),
            (
                "multi_shipments_50",
                {
                    "sort": "container_number",
                    "filters": [
                        {
                            "field": "shipment_id",
                            "values": [uid(i) for i in range(1, 51)],
                        }
                    ],
                },
                [[uid(i) for i in range(1, 51)]],
                False,
            ),
            (
                "multi_combined",
                {
                    "sort": "container_number",
                    "filters": [
                        {"field": "type", "values": ["container"]},
                        {"field": "container_type", "values": ["20DC", "40HC"]},
                        {"field": "hazardous", "values": ["true"]},
                    ],
                },
                [["container"], ["20DC", "40HC"], ["true"]],
                False,
            ),
            ("values_first", {"request": {}, "facet": "container_number"}, [], False),
            (
                "values_next",
                {"request": {}, "facet": "container_number", "after": "mscu0100001"},
                ["mscu0100001"],
                False,
            ),
            (
                "values_search",
                {"request": {}, "facet": "container_number", "query": "00012"},
                ["%00012%"],
                False,
            ),
            (
                "values_with_other_filter",
                {
                    "request": {
                        "filters": [{"field": "container_type", "values": ["40HC"]}]
                    },
                    "facet": "type",
                },
                [["40HC"]],
                False,
            ),
            ("shipment_values", {"request": {}, "facet": "shipment_id"}, [], False),
        ]
    if service == "shipments":
        return [
            ("first_page", {"sort": "shipment_date", "descending": True}, [], True),
            ("number_order", {"sort": "number"}, [], True),
            (
                "combined",
                {
                    "sort": "shipment_date",
                    "descending": True,
                    "customer_id": uid(42),
                    "statuses": ["Новая"],
                    "date_from": "2026-01-01",
                    "date_to": "2026-01-31",
                },
                [uid(42), ["Новая"], "2026-01-01", "2026-01-31"],
                True,
            ),
            (
                "empty_search",
                {"sort": "number", "number_query": "never-matches"},
                ["%never-matches%"],
                True,
            ),
            (
                "multi_numbers_20",
                {
                    "sort": "number",
                    "filters": [
                        {
                            "field": "number",
                            "values": ["S-" + str(i) for i in range(1, 21)],
                        }
                    ],
                },
                [["S-" + str(i) for i in range(1, 21)]],
                False,
            ),
            ("values_first", {"request": {}, "facet": "number"}, [], False),
            (
                "values_next",
                {"request": {}, "facet": "number", "after": "s-50000"},
                ["s-50000"],
                False,
            ),
            (
                "values_search",
                {"request": {}, "facet": "number", "query": "1234"},
                ["%1234%"],
                False,
            ),
            (
                "status_values",
                {"request": {"customer_id": uid(42)}, "facet": "status"},
                [uid(42)],
                False,
            ),
        ]
    return [
        ("first_page", {"sort": "name"}, [False], True),
        (
            "combined",
            {"query": "alpha", "role": "customer", "country_code": "RU"},
            ["%alpha%", "customer", False, "RU"],
            True,
        ),
        ("empty_search", {"query": "never-matches"}, ["%never-matches%", False], True),
        (
            "multi_countries",
            {"filters": [{"field": "country_code", "values": ["RU", "CN"]}]},
            [False, ["RU", "CN"]],
            False,
        ),
        (
            "multi_roles",
            {"filters": [{"field": "roles", "values": ["carrier", "customer"]}]},
            [False, ["carrier", "customer"]],
            False,
        ),
        ("values_first", {"request": {}, "facet": "name"}, [False], False),
        (
            "values_search",
            {"request": {}, "facet": "name", "query": "1234"},
            [False, "%1234%"],
            False,
        ),
        (
            "role_values",
            {"request": {"country_code": "RU"}, "facet": "roles"},
            [False, "RU"],
            False,
        ),
    ]


def literal(value, kind):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, list):
        return (
            "ARRAY[" + ",".join(literal(item, "TEXT") for item in value) + "]::" + kind
        )
    if isinstance(value, int):
        return str(value)
    return "'" + str(value).replace("'", "''") + "'"


def percentile(values, p):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * p) - 1)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--service",
        choices=["shipments", "transportations", "counterparties"],
        required=True,
    )
    parser.add_argument("--rows", type=int)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--query-renderer", required=True)
    parser.add_argument("--baseline-renderer")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.rows = args.rows or (1000000 if args.service == "transportations" else 100000)
    if not 1000 <= args.rows <= 5000000 or not 5 <= args.repetitions <= 100:
        parser.error("Ожидается 1000–5000000 строк, 5–100 повторов")
    docker = shutil.which("docker")
    if not docker:
        parser.error("Нужен Docker")
    name = "broker-column-bench-" + uuid.uuid4().hex[:10]

    def run(*command, **kwargs):
        result = subprocess.run(
            [docker, *command], capture_output=True, text=True, **kwargs
        )
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout

    def sql(text):
        return run(
            "exec",
            "-i",
            name,
            "psql",
            "-XAtq",
            "-h",
            "127.0.0.1",
            "-U",
            "postgres",
            "-v",
            "ON_ERROR_STOP=1",
            input=text,
        )

    started = False
    try:
        run(
            "run",
            "--rm",
            "-d",
            "--name",
            name,
            "--cpus",
            "2",
            "--memory",
            "2g",
            "-e",
            "POSTGRES_HOST_AUTH_METHOD=trust",
            "postgres:16.14-alpine",
            "-c",
            "shared_buffers=256MB",
            "-c",
            "work_mem=16MB",
        )
        started = True
        for _ in range(200):
            if (
                subprocess.run(
                    [
                        docker,
                        "exec",
                        name,
                        "pg_isready",
                        "-h",
                        "127.0.0.1",
                        "-U",
                        "postgres",
                    ],
                    capture_output=True,
                ).returncode
                == 0
            ):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("PostgreSQL не запустился")
        migrations = sorted(
            (ROOT / f"services/{args.service}/postgresql/schemas/db_1").glob("V*.sql")
        )
        sql("\n".join(path.read_text() for path in migrations))
        if args.service == "transportations":
            seed = f"""INSERT INTO transportations (organization_id,id,shipment_id,ordinal,container_type_code,container_ownership,container_number,type,hazardous)
            SELECT CASE WHEN i<={args.rows*4//5} THEN '{ORG}'::uuid ELSE md5('other')::uuid END,md5(i::text)::uuid,md5((i%10000)::text)::uuid,1,
            CASE WHEN i%3=0 THEN '20DC' ELSE '40HC' END,CASE WHEN i%7=0 THEN 'SOC' ELSE 'COC' END,
            CASE WHEN i%10=0 THEN NULL ELSE 'MSCU'||lpad((i%250000)::text,7,'0') END,
            CASE WHEN i%10=0 THEN 'rail' ELSE 'container' END,i%100=1 FROM generate_series(1,{args.rows}) i;"""
        else:
            spec = importlib.util.spec_from_file_location(
                "old_registry_bench", ROOT / "scripts/benchmark-registry-lists.py"
            )
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            seed = module.workload(args.service, args.rows)[0]
        sql(seed + f"\nVACUUM ANALYZE {args.service};")
        report = {
            "service": args.service,
            "rows": args.rows,
            "rows_largest_organization": args.rows * 4 // 5,
            "repetitions": args.repetitions,
            "postgres": sql("SELECT version();").strip(),
            "platform": run(
                "version", "--format", "{{.Server.Os}}/{{.Server.Arch}}"
            ).strip(),
            "limits": {
                "cpus": 2,
                "memory_bytes": 2147483648,
                "shared_buffers": "256MB",
                "work_mem": "16MB",
            },
            "scope": "SQL only; warm cache; no HTTP, label lookup, concurrent users or writers",
            "cases": {},
        }
        seen = {}
        for label, renderer in [
            ("before", args.baseline_renderer),
            ("after", args.query_renderer),
        ]:
            if not renderer:
                continue
            report["cases"][label] = {}
            for case, request, values, has_baseline in cases(args.service):
                if label == "before" and not has_baseline:
                    continue
                statement = subprocess.run(
                    shlex.split(renderer),
                    input=json.dumps(request),
                    capture_output=True,
                    text=True,
                    check=True,
                ).stdout
                bindings = {
                    int(i): t
                    for i, t in re.findall(r"\$(\d+)::([A-Z]+(?:\[\])?)", statement)
                }
                arguments = [ORG, *values, 51]
                if len(arguments) != len(bindings):
                    raise RuntimeError(
                        f"{case}: параметров {len(arguments)}, SQL ожидает {bindings}"
                    )
                prepared = (
                    "PREPARE registry("
                    + ",".join(bindings[i] for i in range(1, len(bindings) + 1))
                    + ") AS "
                    + statement.rstrip().rstrip(";")
                    + ";"
                )
                execute = (
                    "EXECUTE registry("
                    + ",".join(
                        literal(value, bindings[i])
                        for i, value in enumerate(arguments, 1)
                    )
                    + ");"
                )
                for mode in ["force_custom_plan", "force_generic_plan"]:
                    prefix = f"SET plan_cache_mode={mode};\n" + prepared + "\n"
                    result = sql(prefix + execute)
                    if (
                        label == "after"
                        and has_baseline
                        and (case, mode) in seen
                        and result != seen[(case, mode)]
                    ):
                        raise RuntimeError(f"{case}: результаты до и после различаются")
                    seen[(case, mode)] = result
                    # Прогрев отдельно от замеров. Каждая серия использует один prepared statement.
                    plans = sql(
                        prefix
                        + "\n".join(
                            "EXPLAIN (ANALYZE,BUFFERS,FORMAT JSON) " + execute
                            for _ in range(args.repetitions + 1)
                        )
                    )
                    decoder = json.JSONDecoder()
                    documents = []
                    rest = plans.strip()
                    while rest:
                        document, end = decoder.raw_decode(rest)
                        documents.append(document[0])
                        rest = rest[end:].lstrip()
                    plans = documents[1:]
                    timings = [plan["Execution Time"] for plan in plans]
                    last = plans[-1]
                    report["cases"][label][case + ":" + mode] = {
                        "p50_ms": statistics.median(timings),
                        "p95_ms": percentile(timings, 0.95),
                        "min_ms": min(timings),
                        "max_ms": max(timings),
                        "planning_p95_ms": percentile(
                            [plan["Planning Time"] for plan in plans], 0.95
                        ),
                        "query": statement,
                        "arguments": arguments,
                        "explain": last,
                        "shared_hit_blocks": last["Plan"].get("Shared Hit Blocks", 0),
                        "shared_read_blocks": last["Plan"].get("Shared Read Blocks", 0),
                        "temp_written_blocks": last["Plan"].get(
                            "Temp Written Blocks", 0
                        ),
                    }
                    print(
                        f"{args.service} {label} {case} {mode}: p50 {statistics.median(timings):.3f}, p95 {percentile(timings,.95):.3f} ms",
                        flush=True,
                    )
        report["indexes"] = json.loads(
            sql(
                f"SELECT json_agg(json_build_object('name',indexrelname,'bytes',pg_relation_size(indexrelid))) FROM pg_stat_user_indexes WHERE relname='{args.service}';"
            )
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    finally:
        if started:
            subprocess.run([docker, "rm", "-f", name], capture_output=True)


if __name__ == "__main__":
    main()
