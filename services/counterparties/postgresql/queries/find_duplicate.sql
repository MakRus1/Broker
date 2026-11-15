-- $1 — организация.
-- $2 — код страны.
-- $3 — налоговый номер.
-- $4 — идентификатор контрагента, исключаемого из поиска.
SELECT name FROM counterparties
WHERE organization_id = $1::UUID AND country_code = $2 AND tax_id = $3
  AND id <> $4::UUID AND tax_id <> ''
LIMIT 1;
