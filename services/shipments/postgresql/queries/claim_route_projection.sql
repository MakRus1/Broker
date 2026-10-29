WITH candidate AS (
    SELECT organization_id, shipment_id
    FROM route_projection_outbox
    WHERE next_attempt_at <= now()
      AND (lease_until IS NULL OR lease_until < now())
    ORDER BY next_attempt_at, shipment_id
    FOR UPDATE SKIP LOCKED
    LIMIT 1
)
UPDATE route_projection_outbox AS delivery
SET lease_until = now() + INTERVAL '30 seconds', lease_token = gen_random_uuid()
FROM candidate
WHERE delivery.organization_id = candidate.organization_id
  AND delivery.shipment_id = candidate.shipment_id
RETURNING delivery.organization_id::TEXT, delivery.shipment_id::TEXT,
          delivery.revision, jsonb_strip_nulls(delivery.route_segments), delivery.lease_token::TEXT;
