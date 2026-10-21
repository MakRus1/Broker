INSERT INTO counterparties (tenant_id, id, name)
VALUES ($1::UUID, $2::UUID, $3)
ON CONFLICT (tenant_id, id) DO NOTHING
RETURNING ROW(id, name)::public.counterparty_v1;
