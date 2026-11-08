ALTER TABLE notifications RENAME COLUMN shipment_id TO entity_id;
ALTER TABLE notifications RENAME COLUMN shipment_number TO entity_number;
ALTER TABLE notifications RENAME COLUMN shipment_revision TO entity_revision;
ALTER TABLE notifications DROP CONSTRAINT notifications_event_type_check;
ALTER TABLE notifications ADD CONSTRAINT notifications_event_type_check
    CHECK (event_type IN ('shipment.route_changed', 'shipment.created',
        'sales.inquiry.shipment_creation_failed'));
