ALTER TABLE notifications
    ADD COLUMN event_type TEXT NOT NULL DEFAULT 'shipment.route_changed'
    CHECK (event_type IN ('shipment.route_changed', 'shipment.created'));
