-- Бизнесовое состояние операции остаётся в creation_operations.
INSERT INTO outbox (topic, message_key, payload, attempts, available_at)
SELECT 'transportations.create', id::TEXT, payload, attempts, next_attempt_at
FROM creation_operations
WHERE status = 'pending';

-- Существующая очередь проекций переносится без ручной повторной постановки.
INSERT INTO outbox (topic, message_key, payload, available_at)
SELECT 'shipment.route.project', organization_id::TEXT || ':' || shipment_id::TEXT,
       jsonb_build_object(
           'organization_id', organization_id,
           'shipment_id', shipment_id,
           'revision', revision,
           'route_segments', jsonb_strip_nulls(route_segments)),
       next_attempt_at
FROM route_projection_outbox;

-- Доставка теперь полностью принадлежит стандартной outbox.
DROP TABLE route_projection_outbox;
DROP INDEX creation_operations_due_idx;
ALTER TABLE creation_operations
    DROP COLUMN payload,
    DROP COLUMN attempts,
    DROP COLUMN next_attempt_at,
    DROP COLUMN lease_until,
    DROP COLUMN lease_token;
DROP TYPE public.delivery_v1;
DROP TYPE public.delivery_v2;
