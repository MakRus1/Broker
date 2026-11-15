-- $1 — имя источника справочника.
-- $2 — идентификатор аренды синхронизации.
-- $3 — интервал между успешными синхронизациями, в секундах.
-- $4 — начальная задержка повтора, в секундах.
-- $5 — максимальная задержка повтора, в секундах.
-- $6 — срок аренды, в миллисекундах.
UPDATE directory_sync
SET lease = $2::UUID,
    lease_until = now() + $6::INTEGER * INTERVAL '1 millisecond',
    next_attempt_at = now() + $6::INTEGER * INTERVAL '1 millisecond',
    last_attempt_at = now(),
    failure_count = failure_count + 1
WHERE name = $1
    AND (lease IS NULL OR COALESCE(lease_until, next_attempt_at) <= now())
    AND CASE
        WHEN failure_count = 0 THEN updated_at IS NULL
            OR updated_at + $3::INTEGER * INTERVAL '1 second' <= now()
        ELSE last_attempt_at + LEAST($5::INTEGER,
            $4::INTEGER * power(2, LEAST(failure_count - 1, 20))) * INTERVAL '1 second' <= now()
    END
RETURNING name;
