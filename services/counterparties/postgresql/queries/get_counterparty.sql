SELECT ROW(id, name)::public.counterparty_v1
FROM counterparties
WHERE tenant_id = $1::UUID AND id = $2::UUID;
