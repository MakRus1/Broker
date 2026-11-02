UPDATE user_preferences
SET value = $5::JSONB,
    revision = revision + 1
WHERE organization_id = $1::UUID
    AND issuer = $2::TEXT
    AND subject = $3::TEXT
    AND key = $4::TEXT
    AND revision = $6::BIGINT
RETURNING key, value, revision;
