INSERT INTO counterparties (organization_id, id, name)
VALUES ($1::UUID, $2::UUID, $3)
ON CONFLICT (organization_id, id) DO NOTHING
RETURNING ROW(id, name)::public.counterparty_v1;
