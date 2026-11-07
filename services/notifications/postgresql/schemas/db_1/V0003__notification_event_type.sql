-- До этой миграции сохранялись только уведомления об изменении маршрута.
ALTER TABLE notifications
    ADD COLUMN event_type TEXT NOT NULL DEFAULT 'shipment.route_changed'
    CHECK (event_type IN ('shipment.route_changed', 'shipment.created'));

ALTER TABLE notifications ALTER COLUMN event_type DROP DEFAULT;
