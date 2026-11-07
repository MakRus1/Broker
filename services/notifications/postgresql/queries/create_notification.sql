INSERT INTO notifications (
    organization_id, recipient_issuer, recipient_subject,
    shipment_id, shipment_number, shipment_revision, event_type
)
VALUES ($1::UUID, $2, $3, $4::UUID, $5, $6, $7)
ON CONFLICT DO NOTHING;
