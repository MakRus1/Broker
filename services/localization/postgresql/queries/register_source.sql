INSERT INTO localization_source AS current (key, ru, en, parameters)
SELECT key, ru, en, parameters
FROM jsonb_to_recordset($1::JSONB) AS source(key TEXT, ru TEXT, en TEXT, parameters TEXT[])
ON CONFLICT (key) DO UPDATE
SET ru = EXCLUDED.ru, en = EXCLUDED.en, updated_at = now()
WHERE (current.ru, current.en) IS DISTINCT FROM (EXCLUDED.ru, EXCLUDED.en);
