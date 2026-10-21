INSERT INTO idempotent_results (organization_id, action, key, request, result)
VALUES ($1::UUID, $2, $3, $4::JSONB, $5::JSONB);
