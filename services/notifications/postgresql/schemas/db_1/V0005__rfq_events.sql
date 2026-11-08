ALTER TABLE notifications DROP CONSTRAINT notifications_event_type_check;
ALTER TABLE notifications ADD CONSTRAINT notifications_event_type_check
    CHECK (event_type IN ('shipment.route_changed', 'shipment.created',
        'sales.inquiry.shipment_creation_failed', 'sales.rfq.assigned', 'sales.rfq.ready'));

-- Разные события одной редакции должны доставляться независимо одному получателю.
DO $$
DECLARE constraint_name TEXT;
BEGIN
    FOR constraint_name IN
        SELECT conname FROM pg_constraint
        WHERE conrelid = 'notifications'::REGCLASS AND contype = 'u'
    LOOP
        EXECUTE format('ALTER TABLE notifications DROP CONSTRAINT %I', constraint_name);
    END LOOP;
END $$;
ALTER TABLE notifications ADD UNIQUE
    (organization_id, recipient_issuer, recipient_subject, entity_id, entity_revision, event_type);
