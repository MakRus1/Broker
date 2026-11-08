SELECT id::TEXT, entity_id::TEXT, entity_number, entity_revision,
    created_at, read_at IS NOT NULL, event_type
FROM notifications
WHERE organization_id = $1::UUID
    AND recipient_issuer = $2
    AND recipient_subject = $3
    AND dismissed_at IS NULL
    AND ($4::UUID IS NULL OR (created_at, id) < (
        SELECT created_at, id FROM notifications
        WHERE id = $4::UUID
            AND organization_id = $1::UUID
            AND recipient_issuer = $2
            AND recipient_subject = $3
    ))
ORDER BY created_at DESC, id DESC
LIMIT $5;
