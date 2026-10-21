-- Захват задания атомарен; сетевые вызовы выполняются после завершения запроса.
WITH candidate AS (
    SELECT tenant_id, id
    FROM creation_operations
    WHERE status = 'pending'
      AND next_attempt_at <= now()
      AND (lease_until IS NULL OR lease_until < now())
    ORDER BY next_attempt_at, id
    FOR UPDATE SKIP LOCKED
    LIMIT 1
), claimed AS (
    UPDATE creation_operations AS operation
    SET
        lease_until = now() + INTERVAL '30 seconds',
        lease_token = gen_random_uuid(),
        attempts = attempts + 1
    FROM candidate
    WHERE operation.tenant_id = candidate.tenant_id
      AND operation.id = candidate.id
    RETURNING operation.tenant_id, operation.id, operation.lease_token, operation.payload
)
SELECT ROW(tenant_id, id, lease_token, payload)::public.delivery_v1
FROM claimed;
