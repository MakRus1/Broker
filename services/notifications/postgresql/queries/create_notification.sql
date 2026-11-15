-- $1 — организация.
-- $2 — issuer токена получателя.
-- $3 — subject получателя.
-- $4 — идентификатор сущности.
-- $5 — номер сущности.
-- $6 — редакция сущности.
-- $7 — тип события.
INSERT INTO notifications (
    organization_id, recipient_issuer, recipient_subject,
    entity_id, entity_number, entity_revision, event_type
)
VALUES ($1::UUID, $2, $3, $4::UUID, $5, $6, $7)
ON CONFLICT DO NOTHING;
