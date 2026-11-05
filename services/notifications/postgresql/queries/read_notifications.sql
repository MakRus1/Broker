UPDATE notifications
SET read_at = COALESCE(read_at, now()),
    dismissed_at = CASE WHEN $5::BOOLEAN THEN COALESCE(dismissed_at, now()) ELSE dismissed_at END
WHERE organization_id = $1::UUID
    AND recipient_issuer = $2
    AND recipient_subject = $3
    AND id = ANY($4::UUID[])
    AND (read_at IS NULL OR ($5::BOOLEAN AND dismissed_at IS NULL))
RETURNING id;
