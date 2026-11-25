-- $1 — организация.
-- $2 — идентификатор поставки.
-- Синхронизирует проверку использования с созданием перевозки вместе с маршрутом.
SELECT pg_advisory_xact_lock(
    hashtextextended('shipment_route_usage:' || $1::UUID::TEXT || ':' || $2::UUID::TEXT, 0)
);
