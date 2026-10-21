SELECT name FROM counterparties
WHERE organization_id = $1::UUID AND country_code = $2 AND tax_id = $3
  AND id <> $4::UUID AND tax_id <> ''
LIMIT 1;
