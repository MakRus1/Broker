INSERT INTO user_preferences (
    organization_id, issuer, subject, key, value, revision
)
VALUES ($1::UUID, $2::TEXT, $3::TEXT, $4::TEXT, $5::JSONB, 1)
ON CONFLICT (organization_id, issuer, subject, key) DO NOTHING
RETURNING key, value, revision;
