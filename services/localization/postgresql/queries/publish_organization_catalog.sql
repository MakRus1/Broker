INSERT INTO organization_localization_catalogs (organization_id, locale, messages, revision, published_at)
VALUES ($1::UUID, $2, $3::JSONB, $4, now())
ON CONFLICT (organization_id, locale) DO UPDATE
SET messages = EXCLUDED.messages, revision = EXCLUDED.revision, published_at = EXCLUDED.published_at;
