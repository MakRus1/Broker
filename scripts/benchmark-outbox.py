#!/usr/bin/env python3
"""Сравнение индексов захвата outbox при перекосе очередей разных типов."""

import argparse
import json
import math
from pathlib import Path
import statistics
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rows", type=int, default=1000000)
    parser.add_argument("--repetitions", type=int, default=30)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.rows < 1000 or not 5 <= args.repetitions <= 100:
        parser.error("Нужно минимум 1000 сообщений и 5–100 повторов")
    name = "broker-outbox-bench-" + uuid.uuid4().hex[:10]

    def run(*command, **kwargs):
        result = subprocess.run(
            ["docker", *command], capture_output=True, text=True, **kwargs
        )
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout

    def sql(statement):
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
            input=statement,
        )

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
            "--shm-size",
            "512m",
            "-e",
            "POSTGRES_HOST_AUTH_METHOD=trust",
            "postgres:16.14-alpine",
            "-c",
            "shared_buffers=256MB",
        )
        for _ in range(100):
            try:
                sql("SELECT 1;")
                break
            except RuntimeError:
                time.sleep(0.1)
        else:
            raise RuntimeError("PostgreSQL не запустился")
        schema = (ROOT / "libs/outbox/postgresql/schema.sql").read_text()
        sql(schema)
        sql(f"""
            INSERT INTO outbox (id, topic, message_key, payload, available_at)
            SELECT md5(n::TEXT)::UUID,
                   CASE WHEN n <= {args.rows - 100} THEN 'bulk' ELSE 'sparse' END,
                   n::TEXT, '{{"value":1}}'::JSONB,
                   now() - CASE WHEN n <= {args.rows - 100}
                       THEN INTERVAL '2 hours' ELSE INTERVAL '1 hour' END
            FROM generate_series(1, {args.rows}) AS n;
            ANALYZE outbox;
        """)
        claim = (ROOT / "libs/outbox/postgresql/queries/claim.sql").read_text()
        report = {
            "rows": args.rows,
            "bulk_rows": args.rows - 100,
            "sparse_rows": 100,
            "repetitions": args.repetitions,
            "limits": {
                "cpus": 2,
                "memory_bytes": 2147483648,
                "shared_buffers": "256MB",
                "shared_memory_bytes": 536870912,
            },
            "postgres": sql("SELECT version();").strip(),
            "scope": "Прогретый SQL захвата, без HTTP, конкуренции и стоимости enqueue",
            "claim_sql": claim,
            "results": {},
        }
        for index, definition, legacy_array in [
            (
                "available_id",
                "CREATE INDEX outbox_due_idx ON outbox (available_at, id)",
                True,
            ),
            (
                "topic_available_id",
                "CREATE INDEX outbox_due_idx ON outbox (topic, available_at, id)",
                False,
            ),
        ]:
            sql("DROP INDEX outbox_due_idx; " + definition + "; ANALYZE outbox;")
            output = {}
            for topic in ("sparse", "missing", "bulk"):
                statement = claim
                if legacy_array:
                    statement = statement.replace(
                        "topic = $1::TEXT", "topic = ANY($1::TEXT[])"
                    )
                value = f"ARRAY['{topic}']" if legacy_array else f"'{topic}'"
                statement = statement.replace("$1", value).replace("$2", "30")

                def measure():
                    return json.loads(
                        sql(
                            "BEGIN; EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) "
                            + statement
                            + " ROLLBACK;"
                        )
                    )[0]

                measure()
                plans = [measure() for _ in range(args.repetitions)]
                durations = sorted(plan["Execution Time"] for plan in plans)
                output[topic] = {
                    "p50_ms": statistics.median(durations),
                    "p95_ms": durations[math.ceil(len(durations) * 0.95) - 1],
                    "max_ms": max(durations),
                    "samples_ms": durations,
                    "explain": plans[-1],
                }
            report["results"][index] = output
            print(
                index,
                {topic: value["p95_ms"] for topic, value in output.items()},
                flush=True,
            )
        statistics_query = (
            (ROOT / "libs/outbox/postgresql/queries/queue_statistics.sql")
            .read_text()
            .replace("$1", "ARRAY['bulk', 'sparse']")
        )

        def measure_statistics():
            return json.loads(
                sql("EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) " + statistics_query)
            )[0]

        measure_statistics()
        statistics_plans = [measure_statistics() for _ in range(args.repetitions)]
        durations = sorted(plan["Execution Time"] for plan in statistics_plans)
        report["queue_statistics"] = {
            "sql": statistics_query,
            "p50_ms": statistics.median(durations),
            "p95_ms": durations[math.ceil(len(durations) * 0.95) - 1],
            "samples_ms": durations,
            "explain": statistics_plans[-1],
        }
        print("queue_statistics", report["queue_statistics"]["p95_ms"], flush=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    finally:
        subprocess.run(["docker", "rm", "-f", name], capture_output=True)


if __name__ == "__main__":
    main()
