INSERT INTO processed_commands (tenant_id, operation_id, payload, result)
VALUES ($1::UUID, $2::UUID, $3::JSONB, $4::JSONB);
