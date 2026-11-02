SELECT key, value, revision
FROM user_preferences
WHERE organization_id = $1::UUID
    AND issuer = $2::TEXT
    AND subject = $3::TEXT
ORDER BY key;
