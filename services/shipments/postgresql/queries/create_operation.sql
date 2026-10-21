INSERT INTO creation_operations (tenant_id, id, item_id, quantity, payload)
VALUES ($1::UUID, $2::UUID, $3::UUID, $4, $5::JSONB);
