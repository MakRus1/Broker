UPDATE processed_commands
SET result = $3::JSONB
WHERE tenant_id = $1::UUID AND operation_id = $2::UUID;
