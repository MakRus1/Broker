-- Контактные лица получают устойчивые идентификаторы для ссылок из запросов продаж.
UPDATE counterparties AS company
SET contacts = (
    SELECT jsonb_agg(
        CASE WHEN contact ? 'id' THEN contact
             ELSE contact || jsonb_build_object('id', gen_random_uuid()::TEXT)
        END ORDER BY position
    )
    FROM jsonb_array_elements(company.contacts) WITH ORDINALITY AS entry(contact, position)
)
WHERE EXISTS (
    SELECT 1 FROM jsonb_array_elements(company.contacts) AS contact
    WHERE NOT contact ? 'id'
);
