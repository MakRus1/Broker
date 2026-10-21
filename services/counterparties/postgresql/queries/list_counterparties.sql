SELECT ROW(id, name)::public.counterparty_v1
FROM counterparties
WHERE organization_id = $1::UUID AND ($2::UUID IS NULL OR id > $2::UUID)
ORDER BY id
LIMIT $3;
