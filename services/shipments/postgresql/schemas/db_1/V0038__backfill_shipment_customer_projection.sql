-- Отдельный ключ сохраняет уже поставленные в очередь версии маршрута.
INSERT INTO outbox (topic, message_key, payload)
SELECT 'shipment.route.project',
       organization_id::TEXT || ':' || id::TEXT || ':customer',
       jsonb_build_object(
           'organization_id', organization_id,
           'shipment_id', id,
           'revision', revision,
           'customer_id', customer_id)
FROM shipments;
