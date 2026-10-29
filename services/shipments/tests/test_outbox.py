"""Общий SQL outbox: lease, устаревшие подтверждения и замена последнего состояния."""

import json
import re
from uuid import uuid4

import pytest


@pytest.fixture
def outbox_sql(pgsql, service_source_dir):
    directory = service_source_dir.parents[1] / "libs/outbox/postgresql/queries"

    def execute(name, *parameters):
        source = (directory / f"{name}.sql").read_text()
        values = []

        def bind(match):
            values.append(parameters[int(match[1]) - 1])
            return "%s"

        statement = re.sub(r"\$(\d+)", bind, source)
        with pgsql["db_1"].cursor() as cursor:
            cursor.execute(statement, values)
            return cursor.fetchall() if cursor.description else cursor.rowcount

    return execute


def enqueue(sql, topic="test", key=None, payload=None, latest=False):
    key = key or str(uuid4())
    sql(
        "enqueue_latest" if latest else "enqueue",
        topic,
        key,
        json.dumps(payload or {"n": 1}),
    )
    return key


def test_claims_are_exclusive_and_scoped_to_registered_topics(outbox_sql):
    sql = outbox_sql
    enqueue(sql, topic="other")
    enqueue(sql)
    enqueue(sql)
    first = sql("claim", ["test"], 30)[0]
    second = sql("claim", ["test"], 30)[0]
    assert first[0] != second[0]
    assert first[1] == second[1] == "test"
    assert sql("claim", ["test"], 30) == []
    assert len(sql("claim", ["other"], 30)) == 1


def test_expired_lease_is_reclaimed_and_old_ack_cannot_remove_it(outbox_sql, pgsql):
    sql = outbox_sql
    enqueue(sql)
    old = sql("claim", ["test"], 30)[0]
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "UPDATE outbox SET available_at=now()-INTERVAL '1 second' WHERE id=%s",
            (old[0],),
        )
    current = sql("claim", ["test"], 30)[0]
    assert current[0] == old[0]
    assert current[3] != old[3]
    assert sql("lock_delivery", old[0], old[3], old[4]) == []
    assert sql("complete", old[0], old[3], old[4]) == 0
    assert sql("retry", old[0], old[3], 2, 60) == 0
    assert len(sql("lock_delivery", current[0], current[3], current[4])) == 1
    assert sql("complete", current[0], current[3], current[4]) == 1


def test_latest_state_invalidates_old_delivery(outbox_sql):
    sql = outbox_sql
    key = enqueue(sql)
    old = sql("claim", ["test"], 30)[0]
    enqueue(sql, key=key, payload={"n": 2}, latest=True)
    assert sql("lock_delivery", old[0], old[3], old[4]) == []
    assert sql("complete", old[0], old[3], old[4]) == 0
    assert sql("retry", old[0], old[3], 2, 60) == 0
    current = sql("claim", ["test"], 30)[0]
    assert current[0] == old[0]
    assert current[2] == {"n": 2}
    assert current[4] == old[4] + 1


def test_enqueue_is_idempotent_while_pending(outbox_sql):
    sql = outbox_sql
    key = enqueue(sql)
    enqueue(sql, key=key, payload={"n": 2})
    assert sql("claim", ["test"], 30)[0][2] == {"n": 1}
    assert sql("claim", ["test"], 30) == []


def test_retry_uses_bounded_delay_and_releases_lease(outbox_sql, pgsql):
    sql = outbox_sql
    enqueue(sql)
    message = sql("claim", ["test"], 30)[0]
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute("UPDATE outbox SET attempts=100 WHERE id=%s", (message[0],))
    assert sql("retry", message[0], message[3], 2, 60) == 1
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute(
            "SELECT lease_token, available_at > now()+INTERVAL '59 seconds', available_at <= now()+INTERVAL '60 seconds' FROM outbox WHERE id=%s",
            (message[0],),
        )
        assert cursor.fetchone() == (None, True, True)
    assert sql("claim", ["test"], 30) == []


def test_enqueue_rolls_back_with_business_transaction(outbox_sql, pgsql):
    with pgsql["db_1"].cursor() as cursor:
        cursor.execute("BEGIN")
        enqueue(outbox_sql)
        cursor.execute("ROLLBACK")
    assert outbox_sql("claim", ["test"], 30) == []
