-- $1 — организация.
-- $2 — идентификатор поставки.
-- Один порядок сериализации локального снимка использования и записи участков этой поставки.
SELECT pg_advisory_xact_lock(
    hashtextextended('shipment_route_usage:' || $1::UUID::TEXT || ':' || $2::UUID::TEXT, 0)
);
