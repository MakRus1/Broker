SELECT id, number
FROM invoices
WHERE organization_id = $1::UUID AND id = ANY($2::UUID[]);
