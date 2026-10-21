SELECT ROW(id, name)::public.counterparty_v1
FROM counterparties
WHERE organization_id = $1::UUID AND id = $2::UUID;
