WITH selected AS (
    SELECT c.*,
        CASE $7::TEXT
            WHEN 'name' THEN lower(name)
            WHEN 'country_code' THEN lower(COALESCE(country_code, ''))
            WHEN 'tax_id' THEN COALESCE(tax_id, '')
            WHEN 'archived' THEN CASE WHEN archived THEN '1' ELSE '0' END
            ELSE ''
        END COLLATE "C" AS sort_key
    FROM counterparties AS c
    WHERE organization_id = $1::UUID
      AND ($2::TEXT IS NULL OR lower(name || ' ' || COALESCE(legal_name, '') || ' ' || COALESCE(tax_id, '')) COLLATE "C" LIKE lower($2))
      AND ($3::TEXT IS NULL OR $3 = ANY(roles))
      AND ($4::TEXT = 'all' OR archived = ($4::TEXT = 'archived'))
      AND ($5::TEXT IS NULL OR country_code = $5::TEXT)
      AND ($11::UUID[] IS NULL OR id = ANY($11::UUID[]))
)
SELECT ROW(
    id, name, revision, archived, legal_name, country_code,
    tax_id, registration_id, legal_address, roles, contacts, kpp, logist
)::public.counterparty_v5, sort_key
FROM selected
WHERE $10::UUID IS NULL OR
    CASE WHEN $8::BOOLEAN THEN (sort_key, id) < ($9::TEXT COLLATE "C", $10::UUID)
         ELSE (sort_key, id) > ($9::TEXT COLLATE "C", $10::UUID) END
ORDER BY
    CASE WHEN NOT $8::BOOLEAN THEN sort_key END ASC,
    CASE WHEN $8::BOOLEAN THEN sort_key END DESC,
    CASE WHEN NOT $8::BOOLEAN THEN id END ASC,
    CASE WHEN $8::BOOLEAN THEN id END DESC
LIMIT $6;
