INSERT INTO localization_catalogs (locale, messages, revision, published_at)
VALUES ($1, $2::JSONB, $3, now())
ON CONFLICT (locale) DO UPDATE
SET messages = EXCLUDED.messages, revision = EXCLUDED.revision, published_at = EXCLUDED.published_at;
